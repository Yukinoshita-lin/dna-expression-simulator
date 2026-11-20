// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yukinoshita-lin

// 控制台单元测试：验证遗传学核心
#include "../src/genetics.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

static int gPass = 0, gFail = 0;

static void check(bool cond, const std::string& name) {
    if (cond) { ++gPass; std::printf("  ok   %s\n", name.c_str()); }
    else      { ++gFail; std::printf("  FAIL %s\n", name.c_str()); }
}

static void expectEq(const std::string& a, const std::string& b, const std::string& name) {
    if (a == b) { ++gPass; std::printf("  ok   %s\n", name.c_str()); }
    else {
        ++gFail;
        std::printf("  FAIL %s\n       got: [%s]\n       exp: [%s]\n", name.c_str(), a.c_str(), b.c_str());
    }
}

int main() {
    using namespace dna;
    std::printf("== 密码子表 ==\n");

    // 独立的标准遗传密码参考表（DNA）
    const std::map<std::string, char> ref = {
        {"TTT",'F'},{"TTC",'F'},{"TTA",'L'},{"TTG",'L'},
        {"CTT",'L'},{"CTC",'L'},{"CTA",'L'},{"CTG",'L'},
        {"ATT",'I'},{"ATC",'I'},{"ATA",'I'},{"ATG",'M'},
        {"GTT",'V'},{"GTC",'V'},{"GTA",'V'},{"GTG",'V'},
        {"TCT",'S'},{"TCC",'S'},{"TCA",'S'},{"TCG",'S'},
        {"CCT",'P'},{"CCC",'P'},{"CCA",'P'},{"CCG",'P'},
        {"ACT",'T'},{"ACC",'T'},{"ACA",'T'},{"ACG",'T'},
        {"GCT",'A'},{"GCC",'A'},{"GCA",'A'},{"GCG",'A'},
        {"TAT",'Y'},{"TAC",'Y'},{"TAA",'*'},{"TAG",'*'},
        {"CAT",'H'},{"CAC",'H'},{"CAA",'Q'},{"CAG",'Q'},
        {"AAT",'N'},{"AAC",'N'},{"AAA",'K'},{"AAG",'K'},
        {"GAT",'D'},{"GAC",'D'},{"GAA",'E'},{"GAG",'E'},
        {"TGT",'C'},{"TGC",'C'},{"TGA",'*'},{"TGG",'W'},
        {"CGT",'R'},{"CGC",'R'},{"CGA",'R'},{"CGG",'R'},
        {"AGT",'S'},{"AGC",'S'},{"AGA",'R'},{"AGG",'R'},
        {"GGT",'G'},{"GGC",'G'},{"GGA",'G'},{"GGG",'G'},
    };

    check(tableCodons().size() == 64, "64 个密码子");
    int stops = 0;
    bool allMatch = true;
    std::map<std::string, int> unique;
    for (const auto& c : tableCodons()) {
        ++unique[c];
        auto it = ref.find(c);
        if (it == ref.end() || codonToAa(c) != it->second) {
            allMatch = false;
            std::printf("       mismatch %s -> %c (ref %c)\n", c.c_str(), codonToAa(c),
                        it == ref.end() ? '?' : it->second);
        }
        if (codonToAa(c) == '*') ++stops;
        if (displayIndexOf(c) < 0) allMatch = false;
    }
    check(unique.size() == 64, "无重复密码子");
    check(allMatch, "全部 64 项与标准遗传密码一致");
    check(stops == 3, "3 个终止密码子");
    check(codonToAa("ATG") == 'M', "ATG -> Met");
    check(codonToAa("TGG") == 'W', "TGG -> Trp");
    check(codonToAa("NNN") == 0, "NNN 无效");
    check(displayCodon(0) == "TTT" && displayCodon(63) == "GGG", "表顺序 TT.. 到 GG..");
    check(std::string(aminoAcid('A').nameZh) == "丙氨酸", "中文氨基酸名");

    std::printf("== 序列解析 ==\n");
    {
        auto r = parseSequence(">demo gene\natg aaa\nTTT  123\nuga\n");
        check(r.ok, "FASTA + 小写 + 数字 解析成功");
        expectEq(r.sequence, "ATGAAATTTTGA", "序列规范化（U->T）");
        check(r.convertedU, "记录 U->T 转换");
    }
    {
        auto r = parseSequence("ATG BXX");
        check(!r.ok && r.errorCode == "invalid", "非法大写字母报错");
        check(r.invalidChars.size() == 2, "去重后列出 2 个非法字母");
    }
    {
        auto r = parseSequence("ATG bxx");
        check(!r.ok && r.errorCode == "invalid", "非法小写字母不得静默忽略");
        check(r.invalidChars.size() == 2 && r.invalidChars[0] == 'B' && r.invalidChars[1] == 'X',
              "非法小写字母规范化后列出");
    }
    {
        auto r = parseSequence("   \n>only header\n");
        check(!r.ok && r.errorCode == "empty", "空序列报错");
    }
    {
        auto r = parseSequence("ATGN");
        check(r.ok && r.hasN, "N 碱基可接受并标记");
    }

    std::printf("== 互补 / 复制 / 转录 ==\n");
    expectEq(complement("ATGCGT"), "TACGCA", "互补链");
    expectEq(reverseComplement("ATGCGT"), "ACGCAT", "反向互补");
    expectEq(transcribe("ATGAAATTTTGA"), "AUGAAAUUUUGA", "转录 T->U");
    {
        std::string coding = "ATGCGTACGTAA";
        std::string templ = complement(coding);
        expectEq(complement(templ), coding, "复制：新链与模板互补复原编码链");
        expectEq(templ, "TACGCATGCATT", "模板链序列");
    }

    std::printf("== 翻译 ==\n");
    {
        std::string seq = "GGATCCATGAAGGTGGCCATCCTGAGCGACTAA";
        auto tr = translate(seq, 0);
        check(tr.ok && tr.hasStart, "找到起始密码子");
        check(tr.startIndex == 6, "起始下标 = 6");
        expectEq(tr.protein, "MKVAILSD", "翻译产物 MKVAILSD");
        check(tr.hasStop && tr.stopIndex == 30 && tr.stopCodon == "TAA", "终止密码子位置正确");
        check(tr.codons.size() == 8, "8 个已翻译密码子");
        check(tr.complete, "完整 ORF");
        check(codonUsageInOrf(tr, "AAG") == 1, "密码子使用计数");
        check(proteinMass(tr.protein) > 800 && proteinMass(tr.protein) < 1000, "分子量数量级");
    }
    {
        auto tr = translate("CCCAAATTT", 0);
        check(!tr.hasStart && tr.protein == "PKF", "无 ATG 时从框首翻译");
        check(!tr.complete, "无终止密码子标记不完整");
    }
    {
        auto tr = translate("ATGAAANNNTAA", 0);
        check(tr.containsUnknown && tr.protein == "MKX", "N 翻译为 X");
        check(tr.hasStop, "N 之后仍可识别终止密码子");
    }
    {
        // 第二个阅读框：跳过 1 个碱基后出现 ATG
        std::string seq = "A" "ATGAAATAA";
        auto tr = translate(seq, 1);
        check(tr.hasStart && tr.startIndex == 1 && tr.protein == "MK", "阅读框 2 起始定位");
    }
    {
        auto tr = translate("ATGTAAGGG", 0);
        check(tr.hasStart && tr.hasStop && tr.stopIndex == 3 && tr.protein == "M", "起始后立即终止（起始甲硫氨酸保留）");
    }

    std::printf("== 统计与导出 ==\n");
    {
        auto st = computeStats("ATGCGGCC");
        check(st.length == 8 && st.countA == 1 && st.countC == 3 && st.countG == 3 && st.countT == 1,
              "碱基计数");
        check(st.gc > 0.749 && st.gc < 0.751, "GC 含量 75%");
    }
    {
        auto st = computeStats("ATGCGGCC");
        std::string fasta = buildDnaExport("ATGCGGCC", st);
        check(fasta.find(">DNA_sequence") == 0, "DNA 导出 FASTA 头");
        check(fasta.find("ATGCGGCC") != std::string::npos, "DNA 导出包含序列");
    }
    {
        auto seq = std::string("GGATCCATGAAGGTGGCCATCCTGAGCGACTAA");
        auto st = computeStats(seq);
        auto tr = translate(seq, 0);
        std::string txt = buildProteinExport(0, false, tr, st, "2026-11-19 20:00");
        check(txt.find(">protein_1") != std::string::npos, "蛋白导出 FASTA 头");
        check(txt.find("MKVAILSD") != std::string::npos, "蛋白导出含序列");
        check(txt.find("终止密码子 TAA") != std::string::npos, "蛋白导出含终止信息");
        check(txt.find("Met Lys Val") != std::string::npos, "蛋白导出含三字母表示");
    }
    {
        auto seq = std::string("ATGAAANNNTAA");
        auto st = computeStats(seq);
        auto tr = translate(seq, 0);
        std::string txt = buildProteinExport(0, false, tr, st, "2026-11-19 20:00");
        check(txt.find("完整分子量：无法估算") != std::string::npos,
              "含 X 时不输出误导性的完整分子量");
        check(txt.find("质量下限") != std::string::npos, "含 X 时明确标注部分质量下限");
    }
    {
        std::string r = randomGene(20);
        auto parsed = parseSequence(r);
        auto tr = translate(parsed.sequence, 0);
        check(parsed.ok && tr.hasStart && tr.hasStop && tr.protein.size() == 21, "随机基因可翻译为 21 aa（含起始 Met）");
    }

    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
