/*
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * Copyright (c) 2007 MIPS Technologies, Inc.
 * Copyright (c) 2020 Barkhausen Institut
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arch/riscv/pagetable.hh"

#include "sim/serialize.hh"

namespace gem5
{

namespace RiscvISA
{

void
TlbEntry::serialize(CheckpointOut &cp) const
{
    SERIALIZE_SCALAR(paddr);
    SERIALIZE_SCALAR(vaddr);
    SERIALIZE_SCALAR(logBytes);
    SERIALIZE_SCALAR(asid);
    SERIALIZE_SCALAR(pte);
    SERIALIZE_SCALAR(lruSeq);
}

void
TlbEntry::unserialize(CheckpointIn &cp)
{
    UNSERIALIZE_SCALAR(paddr);
    UNSERIALIZE_SCALAR(vaddr);
    UNSERIALIZE_SCALAR(logBytes);
    UNSERIALIZE_SCALAR(asid);
    UNSERIALIZE_SCALAR(pte);
    UNSERIALIZE_SCALAR(lruSeq);
}



#if MPT_ENABLED

// 获取当前层级的“单页大小”    运行时获取页大小, 普通的全局 helper 函数，不是属于某个类或结构体的成员函数，放在命名空间外部
uint64_t getPageSizeForLevel(int level) {
    switch (level) {
        case 0: return MPT_LEAF_L0_PAGE_SIZE;
        case 1: return MPT_LEAF_L1_PAGE_SIZE;
        case 2: return MPT_LEAF_L2_PAGE_SIZE;
        case 3: return MPT_LEAF_L3_PAGE_SIZE;
        default: return 0;//or panic
    }
}

// 获取当前层级的 MPTE 区域大小（16 个页）
uint64_t getRegionSizeForLevel(int level) {
    return MPT_NUM_PERMS * getPageSizeForLevel(level);
}

uint8_t log2floor(uint64_t x) {
    uint8_t r = 0;
    while (x >>= 1) ++r;
    return r;
}



MPTE52::MPTE52() : raw(0) {}// 默认构造函数（无效项）

MPTE52::MPTE52(uint64_t val) : raw(val) {}// 用原始值构造

bool MPTE52::isValid() const { return raw & 0x1; } // 是否有效

bool MPTE52::isLeaf() const { return raw & 0x2; } // 是否为叶子

bool MPTE52::getN() const { return (raw >> 63) & 0x1; } // N 位（bit 63）

// 下一层页表的物理页号（非叶子时使用）
Addr MPTE52::nextLevelPPN() const {
    return (raw >> 10) & 0x000FFFFFFFFFFFFF; // bits 10~61
}

// 下一层页表物理地址（按 4KB 页对齐）
Addr MPTE52::nextLevelPAddr() const {
    return nextLevelPPN() << 12;   //2^12=4KB
}

// 获取第 pi 个页的权限（pi ∈ [0, 15]）
uint8_t MPTE52::perms(uint8_t pi) const {
    // 若启用了 napot，返回统一权限（使用 perms[0]）
    if (getN())
        return (raw >> 2) & MPT_PERM_MASK;

    // 否则返回第 pi 项权限
    if (pi >= MPT_NUM_PERMS) return 0;
    return (raw >> (2 + pi * MPT_PERM_BITS_PER_ENTRY)) & MPT_PERM_MASK;//2是因为最后两位分别是valid和leaf
}






//命名空间级别的工具函数，不在 struct 里面
bool checkMPTEPermissions(const MPTE52 &mpte, BaseMMU::Mode mode, Addr range_offset, int level)
{
    if (!mpte.isValid() || !mpte.isLeaf())
        return false;

    // 当前层的页大小
    uint64_t pageSize = getPageSizeForLevel(level);         // e.g. 2MB for level=1
    uint8_t pi = (range_offset / pageSize) & 0xF;            // 选择第几个页的权限

    uint8_t perm = mpte.perms(pi);

    switch (mode) {
        case BaseMMU::Read:    return perm & MPT_PERM_R;
        case BaseMMU::Write:   return perm & MPT_PERM_W;
        case BaseMMU::Execute: return perm & MPT_PERM_X;
        default: return false;
    }
}



#endif // MPT_ENABLED












} // namespace RiscvISA
} // namespace gem5
