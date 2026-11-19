// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

#include "genetics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <sstream>
#include <unordered_map>

namespace dna {

// ================================================================ 氨基酸表
static const AminoAcid kAminoAcids[] = {
    {'A', "Ala", "丙氨酸",   "nonpolar", "非极性"},
    {'R', "Arg", "精氨酸",   "basic",    "碱性"},
    {'N', "Asn", "天冬酰胺", "polar",    "极性"},
    {'D', "Asp", "天冬氨酸", "acidic",   "酸性"},
    {'C', "Cys", "半胱氨酸", "polar",    "极性"},
    {'Q', "Gln", "谷氨酰胺", "polar",    "极性"},
    {'E', "Glu", "谷氨酸",   "acidic",   "酸性"},
    {'G', "Gly", "甘氨酸",   "nonpolar", "非极性"},
    {'H', "His", "组氨酸",   "basic",    "碱性"},
    {'I', "Ile", "异亮氨酸", "nonpolar", "非极性"},
    {'L', "Leu", "亮氨酸",   "nonpolar", "非极性"},
    {'K', "Lys", "赖氨酸",   "basic",    "碱性"},
    {'M', "Met", "甲硫氨酸", "nonpolar", "非极性"},
    {'F', "Phe", "苯丙氨酸", "nonpolar", "非极性"},
    {'P', "Pro", "脯氨酸",   "nonpolar", "非极性"},
    {'S', "Ser", "丝氨酸",   "polar",    "极性"},
    {'T', "Thr", "苏氨酸",   "polar",    "极性"},
    {'W', "Trp", "色氨酸",   "nonpolar", "非极性"},
    {'Y', "Tyr", "酪氨酸",   "polar",    "极性"},
    {'V', "Val", "缬氨酸",   "nonpolar", "非极性"},
    {'*', "Stop", "终止",    "stop",     "终止"},
    {'X', "Xaa", "未知",     "unknown",  "未知"},
};

const AminoAcid& aminoAcid(char letter) {
    for (const auto& a : kAminoAcids)
        if (a.letter == letter) return a;
    return kAminoAcids[sizeof(kAminoAcids) / sizeof(kAminoAcids[0]) - 1];
}

const char* propertyColorKey(const char* property) { return property; }
bool isStop(char aa) { return aa == '*'; }

// ================================================================ 密码子表
static const char* kBaseOrder = "TCAG";

const std::vector<std::string>& codonFamilyPrefixes() {
    static std::vector<std::string> prefixes;
    if (prefixes.empty()) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                prefixes.push_back(std::string(1, kBaseOrder[i]) + kBaseOrder[j]);
    }
    return prefixes;
}

static std::string decodeCodon(int index) {
    if (index < 0 || index >= 64) return std::string();
    const std::string& prefix = codonFamilyPrefixes()[index / 4];
    return prefix + kBaseOrder[index % 4];
}

const std::vector<std::string>& tableCodons() {
    static std::vector<std::string> codons;
    if (codons.empty()) {
        for (int i = 0; i < 64; ++i) codons.push_back(decodeCodon(i));
    }
    return codons;
}

std::string displayCodon(int index) { return decodeCodon(index); }

int displayIndexOf(const std::string& codon) {
    const auto& all = tableCodons();
    for (int i = 0; i < 64; ++i)
        if (all[i] == codon) return i;
    return -1;
}

// ACGT 字典序（A=0,C=1,G=2,T=3）下的 64 个氨基酸，标准遗传密码
static const char kAcgtCode[65] =
    "KNKNTTTTRSRSIIMIQHQHPPPPRRRRLLLLEDEDAAAAGGGGVVVV*Y*YSSSS*CWCLFLF";

char codonToAa(const std::string& codon) {
    if (codon.size() != 3) return 0;
    int idx = 0;
    for (char c : codon) {
        int v = -1;
        switch (c) {
            case 'A': v = 0; break;
            case 'C': v = 1; break;
            case 'G': v = 2; break;
            case 'T': v = 3; break;
            default: return 0;
        }
        idx = idx * 4 + v;
    }
    return kAcgtCode[idx];
}

// ================================================================ 序列解析
ParseResult parseSequence(const std::string& raw) {
    ParseResult r;
    std::string out;
    out.reserve(raw.size());
    bool invalidSeen[256] = {false};
    int invalidCount = 0;

    std::size_t lineStart = 0;
    while (lineStart <= raw.size()) {
        std::size_t lineEnd = raw.find('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = raw.size();
        std::size_t p = lineStart;
        while (p < lineEnd && (raw[p] == ' ' || raw[p] == '\t' || raw[p] == '\r')) ++p;
        bool header = (p < lineEnd && (raw[p] == '>' || raw[p] == ';'));
        if (!header) {
            for (std::size_t i = lineStart; i < lineEnd; ++i) {
                unsigned char c = (unsigned char)raw[i];
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
                if (c >= '0' && c <= '9') continue;
                if (c == '-' || c == '.' || c == '_') continue;   // 常见分隔符
                char up = (char)std::toupper(c);
                if (up == 'A' || up == 'C' || up == 'G' || up == 'T') {
                    out.push_back(up);
                } else if (up == 'U') {
                    out.push_back('T');
                    r.convertedU = true;
                } else if (up == 'N') {
                    out.push_back('N');
                    r.hasN = true;
                } else if (c >= 'A' && c <= 'Z') {
                    if (!invalidSeen[(unsigned char)up] && r.invalidChars.size() < 8) {
                        r.invalidChars.push_back(up);
                        ++invalidCount;
                    }
                    invalidSeen[(unsigned char)up] = true;
                } else {
                    // 其他符号（如引号、编号）忽略
                }
            }
        }
        if (lineEnd == raw.size()) break;
        lineStart = lineEnd + 1;
    }

    r.sequence = out;
    r.ok = !out.empty() && r.invalidChars.empty();
    if (out.empty()) r.errorCode = "empty";
    else if (!r.invalidChars.empty()) r.errorCode = "invalid";
    return r;
}

SequenceStats computeStats(const std::string& seq) {
    SequenceStats s;
    s.length = (int)seq.size();
    for (char c : seq) {
        switch (c) {
            case 'A': ++s.countA; break;
            case 'C': ++s.countC; break;
            case 'G': ++s.countG; break;
            case 'T': ++s.countT; break;
            case 'N': ++s.countN; break;
            default: break;
        }
    }
    int acgt = s.countA + s.countC + s.countG + s.countT;
    s.gc = acgt > 0 ? (double)(s.countG + s.countC) / acgt : 0.0;
    s.hasN = s.countN > 0;
    return s;
}

std::string complement(const std::string& seq) {
    std::string out;
    out.reserve(seq.size());
    for (char c : seq) {
        switch (c) {
            case 'A': out.push_back('T'); break;
            case 'T': out.push_back('A'); break;
            case 'C': out.push_back('G'); break;
            case 'G': out.push_back('C'); break;
            case 'U': out.push_back('A'); break;
            case 'N': out.push_back('N'); break;
            default:  out.push_back('N'); break;
        }
    }
    return out;
}

std::string reverseString(const std::string& s) { return std::string(s.rbegin(), s.rend()); }

std::string reverseComplement(const std::string& seq) { return reverseString(complement(seq)); }

std::string transcribe(const std::string& coding) {
    std::string rna;
    rna.reserve(coding.size());
    for (char c : coding) rna.push_back(c == 'T' ? 'U' : c);
    return rna;
}

// ================================================================ 翻译
TranslationResult translate(const std::string& coding, int frame) {
    TranslationResult r;
    r.frame = std::max(0, std::min(2, frame));
    if (coding.empty()) return r;
    r.ok = true;

    const std::size_t n = coding.size();
    std::size_t start = std::string::npos;
    for (std::size_t i = (std::size_t)r.frame; i + 3 <= n; i += 3) {
        if (coding.compare(i, 3, "ATG") == 0) { start = i; break; }
    }
    r.hasStart = (start != std::string::npos);
    std::size_t pos = r.hasStart ? start : (std::size_t)r.frame;
    r.startIndex = (int)pos;
    r.untranslatedBases = (int)pos;

    for (; pos + 3 <= n; pos += 3) {
        std::string codon = coding.substr(pos, 3);
        if (codon.find('N') != std::string::npos) {
            r.containsUnknown = true;
            r.codons.push_back(codon);
            r.codonPos.push_back((int)pos);
            r.protein.push_back('X');
            continue;
        }
        char aa = codonToAa(codon);
        if (aa == 0) break;                       // 理论上不会发生（序列已校验）
        if (aa == '*') {
            r.hasStop = true;
            r.stopIndex = (int)pos;
            r.stopCodon = codon;
            break;
        }
        r.codons.push_back(codon);
        r.codonPos.push_back((int)pos);
        r.protein.push_back(aa);
    }
    r.complete = r.hasStop;
    return r;
}

static double residueMass(char aa) {
    switch (aa) {
        case 'A': return 71.0788;   case 'R': return 156.1875;
        case 'N': return 114.1038;  case 'D': return 115.0886;
        case 'C': return 103.1388;  case 'E': return 129.1155;
        case 'Q': return 128.1307;  case 'G': return 57.0519;
        case 'H': return 137.1411;  case 'I': return 113.1594;
        case 'L': return 113.1594;  case 'K': return 128.1741;
        case 'M': return 131.1926;  case 'F': return 147.1766;
        case 'P': return 97.1167;   case 'S': return 87.0782;
        case 'T': return 101.1051;  case 'W': return 186.2132;
        case 'Y': return 163.1760;  case 'V': return 99.1326;
        default:  return 0.0;
    }
}

double proteinMass(const std::string& protein) {
    if (protein.empty()) return 0.0;
    double m = 18.01524;   // H2O
    for (char aa : protein) m += residueMass(aa);
    return m;
}

std::string proteinTriplets(const std::string& protein) {
    std::string out;
    for (std::size_t i = 0; i < protein.size(); ++i) {
        if (i) out.push_back(' ');
        out += aminoAcid(protein[i]).code3;
    }
    return out;
}

int codonUsageInOrf(const TranslationResult& tr, const std::string& codon) {
    int count = 0;
    for (const auto& c : tr.codons) if (c == codon) ++count;
    if (tr.hasStop && tr.stopCodon == codon) ++count;
    return count;
}

// ================================================================ 导出
std::string wrapText(const std::string& s, std::size_t width) {
    if (s.empty()) return std::string();
    std::string out;
    for (std::size_t i = 0; i < s.size(); i += width) {
        if (i) out.push_back('\n');
        out += s.substr(i, width);
    }
    return out;
}

std::string buildDnaExport(const std::string& coding, const SequenceStats& stats) {
    std::ostringstream os;
    os << ">DNA_sequence | length=" << stats.length << " bp | GC=";
    os.setf(std::ios::fixed);
    os.precision(1);
    os << (stats.gc * 100.0) << "% | coding strand 5'-3'\n";
    os << wrapText(coding, 60) << "\n";
    return os.str();
}

std::string buildProteinExport(const std::string& coding, int frame, bool revcomp,
                               const TranslationResult& tr, const SequenceStats& stats,
                               const std::string& datetimeUtf8) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(1);
    os << "; DNA 表达模拟器 · 蛋白质序列导出\n";
    if (!datetimeUtf8.empty()) os << "; 导出时间：" << datetimeUtf8 << "\n";
    os << "; 来源：编码链 5'->3' " << (revcomp ? "（反向互补）" : "（正向）")
       << "，DNA 长度 " << stats.length << " bp，GC " << (stats.gc * 100.0) << "%\n";
    os << "; 阅读框：+" << (frame + 1);
    if (tr.hasStart) os << "，起始密码子 ATG 位置 " << (tr.startIndex + 1);
    else os << "，未找到起始密码子 ATG";
    if (tr.hasStop) {
        std::string rnaStop = tr.stopCodon;
        for (auto& c : rnaStop) if (c == 'T') c = 'U';
        os << "，终止密码子 " << tr.stopCodon << "（" << rnaStop << "）位置 " << (tr.stopIndex + 1);
    } else {
        os << "，未找到终止密码子（序列可能不完整）";
    }
    os << "\n";
    os << "; 氨基酸数：" << tr.protein.size() << " aa";
    if (!tr.protein.empty()) os << "，预测分子量：" << proteinMass(tr.protein) << " Da";
    os << "\n";
    if (tr.containsUnknown) os << "; 注：序列含 N，对应位置以 X 表示未知氨基酸\n";
    os << ">protein_1\n";
    os << (tr.protein.empty() ? std::string("(empty)") : wrapText(tr.protein, 60)) << "\n";
    if (!tr.protein.empty()) {
        os << "\n; 三字母表示：\n";
        std::string triplets = proteinTriplets(tr.protein);
        os << "; " << wrapText(triplets, 64) << "\n";
    }
    (void)coding;
    return os.str();
}

std::string randomGene(int codonCount) {
    static std::mt19937 rng((unsigned)std::random_device{}());
    static const char* bases = "ACGT";
    if (codonCount < 1) codonCount = 1;
    if (codonCount > 400) codonCount = 400;

    const auto& codons = tableCodons();
    std::vector<std::string> sense;
    for (const auto& c : codons) {
        char aa = codonToAa(c);
        if (aa != '*' && aa != 0) sense.push_back(c);
    }
    std::uniform_int_distribution<size_t> anyBase(0, 3);
    std::uniform_int_distribution<size_t> anyCodon(0, sense.size() - 1);
    std::uniform_int_distribution<int> utrLen(6, 21);

    std::string seq;
    int pre = utrLen(rng);
    pre -= pre % 3;   // 让 5' UTR 长度为 3 的倍数，ORF 落在第 1 阅读框
    for (int i = 0; i < pre; ++i) {
        char b = bases[anyBase(rng)];
        // 避免 5' UTR 内意外产生 ATG，保证首个 ATG 就是目标起始密码子
        if (b == 'G' && seq.size() >= 2 && seq[seq.size() - 1] == 'T' &&
            seq[seq.size() - 2] == 'A') {
            b = 'C';
        }
        seq.push_back(b);
    }
    // 防止 UTR 末位 A 与起始密码子跨边界拼出前置 ATG
    if (!seq.empty() && seq.back() == 'A') seq.back() = 'C';
    seq += "ATG";
    for (int i = 0; i < codonCount; ++i) seq += sense[anyCodon(rng)];
    static const char* stops[] = {"TAA", "TAG", "TGA"};
    seq += stops[anyBase(rng) % 3];
    int post = utrLen(rng);
    for (int i = 0; i < post; ++i) seq.push_back(bases[anyBase(rng)]);
    return seq;
}

} // namespace dna
