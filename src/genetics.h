// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// genetics.h — 遗传信息流核心模型（纯 C++，无平台依赖）
#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace dna {

// ---------------------------------------------------------------- 氨基酸
struct AminoAcid {
    char letter;            // 单字母代码，'*' = 终止，'X' = 未知
    const char* code3;      // 三字母代码（英文）
    const char* nameZh;     // 中文名（UTF-8）
    const char* property;   // nonpolar / polar / acidic / basic / stop / unknown
    const char* propertyZh; // 性质中文（UTF-8）
};

const AminoAcid& aminoAcid(char letter);          // 'A'..'Y'、'*'、'X'
const char* propertyColorKey(const char* property);
bool isStop(char aa);

// ---------------------------------------------------------------- 密码子表
// 64 个密码子，按经典表格顺序：前两位分族 T T / T C / ... / G G，第三位 T C A G
const std::vector<std::string>& tableCodons();
std::string displayCodon(int index);              // 0..63，DNA 字母
int displayIndexOf(const std::string& codon);     // 0..63 或 -1
char codonToAa(const std::string& codon);         // 'M'..，'*' 终止，0 无效（含 N 等）
const std::vector<std::string>& codonFamilyPrefixes(); // "TT","TC",...,"GG"

// ---------------------------------------------------------------- 序列解析
struct ParseResult {
    std::string sequence;       // 规范化后的编码链（大写、T 代替 U）
    bool ok = false;
    bool convertedU = false;
    bool hasN = false;
    std::vector<char> invalidChars;   // 不支持的字母（去重，最多 8 个）
    std::string errorCode;            // "empty" / "invalid" / ""
};

ParseResult parseSequence(const std::string& raw);

struct SequenceStats {
    int length = 0;
    int countA = 0, countC = 0, countG = 0, countT = 0, countN = 0;
    double gc = 0.0;            // GC / (A+C+G+T)
    bool hasN = false;
    bool empty() const { return length == 0; }
};

SequenceStats computeStats(const std::string& seq);
std::string complement(const std::string& seq);          // 同向互补（非反向）
std::string reverseComplement(const std::string& seq);
std::string transcribe(const std::string& coding);       // T -> U
std::string reverseString(const std::string& s);

// ---------------------------------------------------------------- 翻译
struct TranslationResult {
    bool ok = false;
    bool hasStart = false;
    bool hasStop = false;
    bool complete = false;
    bool containsUnknown = false;
    bool ambiguousStop = false;
    int frame = 0;                 // 0,1,2
    int startIndex = -1;           // AUG 的 0 基下标
    int stopIndex = -1;            // 终止密码子首位的 0 基下标
    int untranslatedBases = 0;
    std::string stopCodon;         // DNA 字母
    std::string protein;           // 单字母序列（不含终止）
    std::vector<std::string> codons;   // 已翻译的密码子（不含终止）
    std::vector<int> codonPos;         // 对应 0 基下标
};

// 从指定阅读框查找第一个 ATG，读到第一个同框终止密码子；无 ATG 时从框首开始
TranslationResult translate(const std::string& coding, int frame);
double proteinMass(const std::string& protein);
std::string proteinTriplets(const std::string& protein);   // "Met Lys Val"
int codonUsageInOrf(const TranslationResult& tr, const std::string& codon);

// ---------------------------------------------------------------- 导出与工具
std::string wrapText(const std::string& s, std::size_t width);
std::string buildDnaExport(const std::string& coding, const SequenceStats& stats);
std::string buildProteinExport(const std::string& coding, int frame, bool revcomp,
                               const TranslationResult& tr, const SequenceStats& stats,
                               const std::string& datetimeUtf8);
std::string randomGene(int codonCount);

} // namespace dna
