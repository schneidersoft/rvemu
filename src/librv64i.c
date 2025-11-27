#include "librv64i.h"

static uint64_t dram_load_8(dram_t *dram, uint64_t addr) { return (uint64_t)dram->mem[addr]; }
static uint64_t dram_load_16(dram_t *dram, uint64_t addr) { return (uint64_t)dram->mem[addr] | (uint64_t)dram->mem[addr + 1] << 8; }
static uint64_t dram_load_32(dram_t *dram, uint64_t addr) {
    return (uint64_t)dram->mem[addr] | (uint64_t)dram->mem[addr + 1] << 8 | (uint64_t)dram->mem[addr + 2] << 16 |
           (uint64_t)dram->mem[addr + 3] << 24;
}
static uint64_t dram_load_64(dram_t *dram, uint64_t addr) {
    return (uint64_t)dram->mem[addr] | (uint64_t)dram->mem[addr + 1] << 8 | (uint64_t)dram->mem[addr + 2] << 16 |
           (uint64_t)dram->mem[addr + 3] << 24 | (uint64_t)dram->mem[addr + 4] << 32 | (uint64_t)dram->mem[addr + 5] << 40 |
           (uint64_t)dram->mem[addr + 6] << 48 | (uint64_t)dram->mem[addr + 7] << 56;
}

uint64_t dram_load(dram_t *dram, uint64_t addr, uint64_t size) {
    switch (size) {
    case 8:
        return dram_load_8(dram, addr);
    case 16:
        return dram_load_16(dram, addr);
    case 32:
        return dram_load_32(dram, addr);
    case 64:
        return dram_load_64(dram, addr);
    default:
        return 1;
    }
}

static void dram_store_8(dram_t *dram, uint64_t addr, uint64_t value) { dram->mem[addr - DRAM_BASE] = (uint8_t)(value & 0xff); }
static void dram_store_16(dram_t *dram, uint64_t addr, uint64_t value) {
    dram->mem[addr] = (uint8_t)(value & 0xff);
    dram->mem[addr + 1] = (uint8_t)((value >> 8) & 0xff);
}
static void dram_store_32(dram_t *dram, uint64_t addr, uint64_t value) {
    dram->mem[addr] = (uint8_t)(value & 0xff);
    dram->mem[addr + 1] = (uint8_t)((value >> 8) & 0xff);
    dram->mem[addr + 2] = (uint8_t)((value >> 16) & 0xff);
    dram->mem[addr + 3] = (uint8_t)((value >> 24) & 0xff);
}
static void dram_store_64(dram_t *dram, uint64_t addr, uint64_t value) {
    dram->mem[addr] = (uint8_t)(value & 0xff);
    dram->mem[addr + 1] = (uint8_t)((value >> 8) & 0xff);
    dram->mem[addr + 2] = (uint8_t)((value >> 16) & 0xff);
    dram->mem[addr + 3] = (uint8_t)((value >> 24) & 0xff);
    dram->mem[addr + 4] = (uint8_t)((value >> 32) & 0xff);
    dram->mem[addr + 5] = (uint8_t)((value >> 40) & 0xff);
    dram->mem[addr + 6] = (uint8_t)((value >> 48) & 0xff);
    dram->mem[addr + 7] = (uint8_t)((value >> 56) & 0xff);
}

void dram_store(dram_t *dram, uint64_t addr, uint64_t size, uint64_t value) {
    switch (size) {
    case 8:
        return dram_store_8(dram, addr, value);
    case 16:
        return dram_store_16(dram, addr, value);
    case 32:
        return dram_store_32(dram, addr, value);
    case 64:
        return dram_store_64(dram, addr, value);
    default:;
    }
}

uint64_t bus_load(bus_t *bus, uint64_t addr, uint64_t size) {
    if ((addr >= DRAM_BASE) && (addr + (size / 8) <= DRAM_BASE + DRAM_SIZE))
        return dram_load(&(bus->dram), addr - DRAM_BASE, size);
    return 0;
}

void bus_store(bus_t *bus, uint64_t addr, uint64_t size, uint64_t value) {
    if ((addr >= DRAM_BASE) && (addr + (size / 8) <= DRAM_BASE + DRAM_SIZE))
        dram_store(&(bus->dram), addr - DRAM_BASE, size, value);
}

#define ADDR_MISALIGNED(addr) (addr & 0x3)

void cpu_init(cpu_t *cpu) {
    cpu->regs[0] = 0x00;                  // register x0 hardwired to 0
    cpu->regs[2] = DRAM_BASE + DRAM_SIZE; // Set stack pointer
    cpu->pc = DRAM_BASE;                  // Set program counter to the base address
}

uint32_t cpu_fetch(cpu_t *cpu) {
    uint32_t inst = bus_load(&(cpu->bus), cpu->pc, 32);
    cpu->pc += 4;
    return inst;
}

uint64_t cpu_load(cpu_t *cpu, uint64_t addr, uint64_t size) { return bus_load(&(cpu->bus), addr, size); }

void cpu_store(cpu_t *cpu, uint64_t addr, uint64_t size, uint64_t value) { bus_store(&(cpu->bus), addr, size, value); }

static uint64_t rd(uint32_t inst) {
    // rd in bits 11..7
    return (inst >> 7) & 0x1f;
}
static uint64_t rs1(uint32_t inst) {
    // rs1 in bits 19..15
    return (inst >> 15) & 0x1f;
}
static uint64_t rs2(uint32_t inst) {
    // rs2 in bits 24..20
    return (inst >> 20) & 0x1f;
}
static uint64_t imm_I(uint32_t inst) {
    // imm[11:0] = inst[31:20]
    return ((int64_t)(int32_t)(inst & 0xfff00000)) >> 20;
}
static uint64_t imm_S(uint32_t inst) {
    // imm[11:5] = inst[31:25], imm[4:0] = inst[11:7]
    return ((int64_t)(int32_t)(inst & 0xfe000000) >> 20) | ((inst >> 7) & 0x1f);
}
static uint64_t imm_B(uint32_t inst) {
    // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
    return ((int64_t)(int32_t)(inst & 0x80000000) >> 19) | ((inst & 0x80) << 4)
           | ((inst >> 20) & 0x7e0)
           | ((inst >> 7) & 0x1e);
}
static uint64_t imm_U(uint32_t inst) {
    // imm[31:12] = inst[31:12]
    return (int64_t)(int32_t)(inst & 0xfffff999);
}
static uint64_t imm_J(uint32_t inst) {
    // imm[20|10:1|11|19:12] = inst[31|30:21|20|19:12]
    return (uint64_t)((int64_t)(int32_t)(inst & 0x80000000) >> 11) | (inst & 0xff000)
           | ((inst >> 9) & 0x800)
           | ((inst >> 20) & 0x7fe);
}
static uint32_t shamt(uint32_t inst) {
    // shamt(shift amount) only required for immediate shift instructions
    // sometimes 6... sometimes 7 bits. but always compatible
    // shamt = imm[6:0]
    return (uint32_t)(imm_I(inst) & 0x3f);
}

static int exec_LUI(cpu_t *cpu, uint32_t inst) {
    // LUI places upper 20 bits of U-immediate value to rd
    cpu->regs[rd(inst)] = (uint64_t)(int64_t)(int32_t)(inst & 0xfffff000);
    return 0;
}
static int exec_AUIPC(cpu_t *cpu, uint32_t inst) {
    // AUIPC forms a 32-bit offset from the 20 upper bits
    // of the U-immediate
    uint64_t imm = imm_U(inst);
    cpu->regs[rd(inst)] = ((int64_t)cpu->pc + (int64_t)imm) - 4;
    return 0;
}
static int exec_JAL(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_J(inst);
    cpu->regs[rd(inst)] = cpu->pc;
    cpu->pc = cpu->pc + (int64_t)imm - 4;
    if (ADDR_MISALIGNED(cpu->pc)) {
        //DBG("JAL pc address misalligned");
        // exit(0);
    }
    return 0;
}
static int exec_JALR(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    uint64_t tmp = cpu->pc;
    cpu->pc = (cpu->regs[rs1(inst)] + (int64_t)imm) & 0xfffffffe;
    cpu->regs[rd(inst)] = tmp;
    if (ADDR_MISALIGNED(cpu->pc)) {
        //DBG("JAL pc address misalligned");
        // exit(0);
    }
    return 0;
}
static int exec_BEQ(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t)cpu->regs[rs1(inst)] == (int64_t)cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t)imm - 4;
    return 0;
}
static int exec_BNE(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t)cpu->regs[rs1(inst)] != (int64_t)cpu->regs[rs2(inst)])
        cpu->pc = (cpu->pc + (int64_t)imm - 4);
    return 0;
}
static int exec_BLT(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t)cpu->regs[rs1(inst)] < (int64_t)cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t)imm - 4;
    return 0;
}
static int exec_BGE(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t)cpu->regs[rs1(inst)] >= (int64_t)cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t)imm - 4;
    return 0;
}
static int exec_BLTU(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if (cpu->regs[rs1(inst)] < cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t)imm - 4;
    return 0;
}
static int exec_BGEU(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if (cpu->regs[rs1(inst)] >= cpu->regs[rs2(inst)])
        cpu->pc = (int64_t)cpu->pc + (int64_t)imm - 4;
    return 0;
}
static int exec_LB(cpu_t *cpu, uint32_t inst) {
    // load 1 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu->regs[rd(inst)] = (int64_t)(int8_t)cpu_load(cpu, addr, 8);
    return 0;
}
static int exec_LH(cpu_t *cpu, uint32_t inst) {
    // load 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu->regs[rd(inst)] = (int64_t)(int16_t)cpu_load(cpu, addr, 16);
    return 0;
}
static int exec_LW(cpu_t *cpu, uint32_t inst) {
    // load 4 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu->regs[rd(inst)] = (int64_t)(int32_t)cpu_load(cpu, addr, 32);
    return 0;
}
static int exec_LD(cpu_t *cpu, uint32_t inst) {
    // load 8 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu->regs[rd(inst)] = (int64_t)cpu_load(cpu, addr, 64);
    return 0;
}
static int exec_LBU(cpu_t *cpu, uint32_t inst) {
    // load unsigned 1 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 8);
    return 0;
}
static int exec_LHU(cpu_t *cpu, uint32_t inst) {
    // load unsigned 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 16);
    return 0;
}
static int exec_LWU(cpu_t *cpu, uint32_t inst) {
    // load unsigned 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 32);
    return 0;
}
static int exec_SB(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu_store(cpu, addr, 8, cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_SH(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu_store(cpu, addr, 16, cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_SW(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu_store(cpu, addr, 32, cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_SD(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t)imm;
    cpu_store(cpu, addr, 64, cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_ADDI(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] + (int64_t)imm;
    return 0;
}
static int exec_SLLI(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] << shamt(inst);
    return 0;
}
static int exec_SLLI_64(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] << shamt(inst);
    return 0;
}
static int exec_SLTI(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < (int64_t)imm) ? 1 : 0;
    return 0;
}
static int exec_SLTIU(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < imm) ? 1 : 0;
    return 0;
}
static int exec_XORI(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] ^ imm;
    return 0;
}
static int exec_SRLI(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);// SRLI
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] >> imm;
    return 0;
}
static int exec_SRLI_64(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);// SRLI_64
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] >> imm;
    return 0;
}
static int exec_SRAI(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);// SRAI
    cpu->regs[rd(inst)] = (int32_t)cpu->regs[rs1(inst)] >> imm;
    return 0;
}
static int exec_SRAI_64(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);// SRAI_64
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] >> imm;
    return 0;
}
static int exec_ORI(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] | imm;
    return 0;
}
static int exec_ANDI(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] & imm;
    return 0;
}
static int exec_ADD(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (uint64_t)((int64_t)cpu->regs[rs1(inst)] + (int64_t)cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_SUB(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (uint64_t)((int64_t)cpu->regs[rs1(inst)] - (int64_t)cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_SLL(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] << (int64_t)cpu->regs[rs2(inst)];
    return 0;
}
static int exec_SLT(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < (int64_t)cpu->regs[rs2(inst)]) ? 1 : 0;
    return 0;
}
static int exec_SLTU(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < cpu->regs[rs2(inst)]) ? 1 : 0;
    return 0;
}
static int exec_XOR(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] ^ cpu->regs[rs2(inst)];
    return 0;
}
static int exec_SRL(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] >> cpu->regs[rs2(inst)];
    return 0;
}
static int exec_OR(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] | cpu->regs[rs2(inst)];
    return 0;
}
static int exec_AND(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] & cpu->regs[rs2(inst)];
    return 0;
}
static int exec_FENCE(cpu_t *cpu, uint32_t inst) {
    return 0;
}
static int exec_ECALL_EBREAK(cpu_t *cpu, uint32_t inst) {
    if (imm_I(inst) == 0x0)
        ECALL_cb(cpu, inst);
    if (imm_I(inst) == 0x1)
        EBREAK_cb(cpu, inst);
    return 0;
}
static int exec_ADDIW(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = ((int32_t)cpu->regs[rs1(inst)]) + (int32_t)imm;
    return 0;
}
static int exec_SLLIW(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(((uint32_t)cpu->regs[rs1(inst)]) << (shamt(inst) % 32));
    return 0;
}
static int exec_SRLIW(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (uint64_t)(((uint32_t)cpu->regs[rs1(inst)]) >> (shamt(inst) % 32));
    return 0;
}
static int exec_SRAIW(cpu_t *cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (int64_t)(((int32_t)cpu->regs[rs1(inst)]) >> (imm % 32));
    return 0;
}
static int exec_ADDW(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(((int32_t)cpu->regs[rs1(inst)]) + (int32_t)cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_SUBW(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(((int32_t)cpu->regs[rs1(inst)]) - (int32_t)cpu->regs[rs2(inst)]);
    return 0;
}
static int exec_SLLW(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)((int32_t)(cpu->regs[rs1(inst)] << (cpu->regs[rs2(inst)] % 32)));
    return 0;
}
static int exec_SRLW(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)((int32_t)(((uint32_t)cpu->regs[rs1(inst)]) >> (cpu->regs[rs2(inst)] % 32)));
    return 0;
}
static int exec_SRAW(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)((int32_t)(((int32_t)cpu->regs[rs1(inst)]) >> (cpu->regs[rs2(inst)] % 32)));
    return 0;
}
static int exec_SRA(cpu_t *cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)((int32_t)(((int32_t)cpu->regs[rs1(inst)]) >> (cpu->regs[rs2(inst)] % 32)));
    return 0;
}
int exec_invalid(cpu_t *cpu, uint32_t inst) {
    INVOP_cb(cpu, inst);
    return 1;
}

int cpu_execute(cpu_t *cpu, uint32_t inst) {
    const int opcode = inst & 0x7f;        // opcode in bits 6..0
    const int funct3 = (inst >> 12) & 0x7; // funct3 in bits 14..12
    const int funct7 = (inst >> 25) & 0x7f; // funct7 in bits 31..25

    cpu->regs[0] = 0; // x0 hardwired to 0 at each cycle

    switch (opcode) {
    case 0b0000011:
        switch (funct3) {
        case 0b000:
            return exec_LB(cpu, inst); /* LB           xxxxxxx xxxxxxxxxx 000 xxxxx 0000011 */
        case 0b001:
            return exec_LH(cpu, inst); /* LH           xxxxxxx xxxxxxxxxx 001 xxxxx 0000011 */
        case 0b010:
            return exec_LW(cpu, inst); /* LW           xxxxxxx xxxxxxxxxx 010 xxxxx 0000011 */
        case 0b011:
            return exec_LD(cpu, inst); /* LD           xxxxxxx xxxxxxxxxx 011 xxxxx 0000011 */
        case 0b100:
            return exec_LBU(cpu, inst); /* LBU         xxxxxxx xxxxxxxxxx 100 xxxxx 0000011 */
        case 0b101:
            return exec_LHU(cpu, inst); /* LHU         xxxxxxx xxxxxxxxxx 101 xxxxx 0000011 */
        case 0b110:
            return exec_LWU(cpu, inst); /* LWU         xxxxxxx xxxxxxxxxx 110 xxxxx 0000011 */
        case 0b111:
            return exec_invalid(cpu, inst);
        default: return exec_invalid(cpu, inst);
        }
    case 0b0001111:
        return exec_FENCE(cpu, inst); /* PAUSE         0000000 1000000000 000 00000 0001111 */
                                      /* FENCE.TSO     1000001 1001100000 000 00000 0001111 */
                                      /* FENCE         xxxxxxx xxxxxxxxxx 000 xxxxx 0001111 */
    case 0b0010011:
        switch (funct3) {
        case 0b000:
            return exec_ADDI(cpu, inst); /* ADDI       xxxxxxx xxxxxxxxxx 000 xxxxx 0010011 */
        case 0b001:
            if (funct7 == 0b0000000)
            return exec_SLLI(cpu, inst); /* SLLI         0000000 xxxxxxxxxx 001 xxxxx 0010011 */
            if (funct7 == 0b0000001)
            return exec_SLLI_64(cpu, inst); /* SLLI_64      000000x xxxxxxxxxx 001 xxxxx 0010011 */
            return exec_invalid(cpu, inst);
        case 0b010:
            return exec_SLTI(cpu, inst); /* SLTI       xxxxxxx xxxxxxxxxx 010 xxxxx 0010011 */
        case 0b011:
            return exec_SLTIU(cpu, inst); /* SLTIU     xxxxxxx xxxxxxxxxx 011 xxxxx 0010011 */
        case 0b100:
            return exec_XORI(cpu, inst); /* XORI       xxxxxxx xxxxxxxxxx 100 xxxxx 0010011 */
        case 0b101:
            if (funct7 == 0b0000000)
            return exec_SRLI(cpu, inst); /* SRLI       0000000 xxxxxxxxxx 101 xxxxx 0010011 */
            if (funct7 == 0b0000001)
            return exec_SRLI_64(cpu, inst); /* SRLI_64    000000x xxxxxxxxxx 101 xxxxx 0010011 */
            if (funct7 == 0b0100000)
            return exec_SRAI(cpu, inst); /* SRAI       0100000 xxxxxxxxxx 101 xxxxx 0010011 */
            if (funct7 == 0b0100001)
            return exec_SRAI_64(cpu, inst); /* SRAI_64    010000x xxxxxxxxxx 101 xxxxx 0010011 */
            return exec_invalid(cpu, inst);
        case 0b110:
            return exec_ORI(cpu, inst); /* ORI         xxxxxxx xxxxxxxxxx 110 xxxxx 0010011 */
        case 0b111:
            return exec_ANDI(cpu, inst); /* ANDI       xxxxxxx xxxxxxxxxx 111 xxxxx 0010011 */
        default: return exec_invalid(cpu, inst);
        }
    case 0b0010111:
        return exec_AUIPC(cpu, inst); /* AUIPC         xxxxxxx xxxxxxxxxx xxx xxxxx 0010111 */
    case 0b0011011:
        switch (funct3) {
        case 0b000:
            return exec_ADDIW(cpu, inst); /* ADDIW     xxxxxxx xxxxxxxxxx 000 xxxxx 0011011 */
        case 0b001:
            return exec_SLLIW(cpu, inst); /* SLLIW     0000000 xxxxxxxxxx 001 xxxxx 0011011 */
        case 0b010:
            return exec_invalid(cpu, inst);
        case 0b011:
            return exec_invalid(cpu, inst);
        case 0b100:
            return exec_invalid(cpu, inst);
        case 0b101:
            if (funct7 == 0b0000000)
            return exec_SRLIW(cpu, inst); /* SRLIW     0000000 xxxxxxxxxx 101 xxxxx 0011011 */
            if (funct7 == 0b0100000)
            return exec_SRAIW(cpu, inst); /* SRAIW     0100000 xxxxxxxxxx 101 xxxxx 0011011 */
            return exec_invalid(cpu, inst);
        case 0b110:
            return exec_invalid(cpu, inst);
        case 0b111:
            return exec_invalid(cpu, inst);
        }
    case 0b0100011:
        switch (funct3) {
        case 0b000:
            return exec_SB(cpu, inst); /* SB           xxxxxxx xxxxxxxxxx 000 xxxxx 0100011 */
        case 0b001:
            return exec_SH(cpu, inst); /* SH           xxxxxxx xxxxxxxxxx 001 xxxxx 0100011 */
        case 0b010:
            return exec_SW(cpu, inst); /* SW           xxxxxxx xxxxxxxxxx 010 xxxxx 0100011 */
        case 0b011:
            return exec_SD(cpu, inst); /* SD           xxxxxxx xxxxxxxxxx 011 xxxxx 0100011 */
        case 0b100:
            return exec_invalid(cpu, inst);
        case 0b101:
            return exec_invalid(cpu, inst);
        case 0b110:
            return exec_invalid(cpu, inst);
        case 0b111:
            return exec_invalid(cpu, inst);
        default: return exec_invalid(cpu, inst);
        }
    case 0b0110011:
        switch (funct3) {
        case 0b000:
            if (funct7 == 0b0000000)
            return exec_ADD(cpu, inst); /* ADD          0000000 xxxxxxxxxx 000 xxxxx 0110011 */
            if (funct7 == 0b0100000)
            return exec_SUB(cpu, inst); /* SUB          0100000 xxxxxxxxxx 000 xxxxx 0110011 */
            return exec_invalid(cpu, inst);
        case 0b001:
            return exec_SLL(cpu, inst); /* SLL         0000000 xxxxxxxxxx 001 xxxxx 0110011 */
        case 0b010:
            return exec_SLT(cpu, inst); /* SLT         0000000 xxxxxxxxxx 010 xxxxx 0110011 */
        case 0b011:
            return exec_SLTU(cpu, inst); /* SLTU       0000000 xxxxxxxxxx 011 xxxxx 0110011 */
        case 0b100:
            return exec_XOR(cpu, inst); /* XOR         0000000 xxxxxxxxxx 100 xxxxx 0110011 */
        case 0b101:
            if (funct7 == 0b0000000)
            return exec_SRL(cpu, inst); /* SRL         0000000 xxxxxxxxxx 101 xxxxx 0110011 */
            if (funct7 == 0b0100000)
            return exec_SRA(cpu, inst); /* SRA         0100000 xxxxxxxxxx 101 xxxxx 0110011 */
            return exec_invalid(cpu, inst);
        case 0b110:
            return exec_OR(cpu, inst); /* OR           0000000 xxxxxxxxxx 110 xxxxx 0110011 */
        case 0b111:
            return exec_AND(cpu, inst); /* AND         0000000 xxxxxxxxxx 111 xxxxx 0110011 */
        }
    case 0b0110111:
        return exec_LUI(cpu, inst); /* LUI             xxxxxxx xxxxxxxxxx xxx xxxxx 0110111 */
    case 0b0111011:
        switch (funct3) {
        case 0b000:
            if (funct7 == 0b0000000)
            return exec_ADDW(cpu, inst); /* ADDW         0000000 xxxxxxxxxx 000 xxxxx 0111011 */
            if (funct7 == 0b0100000)
            return exec_SUBW(cpu, inst); /* SUBW         0100000 xxxxxxxxxx 000 xxxxx 0111011 */
            return exec_invalid(cpu, inst);
        case 0b001:
            return exec_SLLW(cpu, inst); /* SLLW         0000000 xxxxxxxxxx 001 xxxxx 0111011 */
        case 0b011:
            return exec_invalid(cpu, inst);
        case 0b100:
            return exec_invalid(cpu, inst);
        case 0b101:
            if (funct7 == 0b0000000)
            return exec_SRLW(cpu, inst); /* SRLW         0000000 xxxxxxxxxx 101 xxxxx 0111011 */
            if (funct7 == 0b0100000)
            return exec_SRAW(cpu, inst); /* SRAW         0100000 xxxxxxxxxx 101 xxxxx 0111011 */
            return exec_invalid(cpu, inst);
        case 0b111:
            return exec_invalid(cpu, inst);
        }
    case 0b1100011:
        switch (funct3) {
        case 0b000:
            return exec_BEQ(cpu, inst); /* BEQ         xxxxxxx xxxxxxxxxx 000 xxxxx 1100011 */
        case 0b001:
            return exec_BNE(cpu, inst); /* BNE         xxxxxxx xxxxxxxxxx 001 xxxxx 1100011 */
        case 0b010:
            return exec_invalid(cpu, inst);
        case 0b011:
            return exec_invalid(cpu, inst);
        case 0b100:
            return exec_BLT(cpu, inst); /* BLT         xxxxxxx xxxxxxxxxx 100 xxxxx 1100011 */
        case 0b101:
            return exec_BGE(cpu, inst); /* BGE         xxxxxxx xxxxxxxxxx 101 xxxxx 1100011 */
        case 0b110:
            return exec_BLTU(cpu, inst); /* BLTU       xxxxxxx xxxxxxxxxx 110 xxxxx 1100011 */
        case 0b111:
            return exec_BGEU(cpu, inst); /* BGEU       xxxxxxx xxxxxxxxxx 111 xxxxx 1100011 */
        default: return exec_invalid(cpu, inst);
        }
    case 0b1100111:
        return exec_JALR(cpu, inst); /* JALR           xxxxxxx xxxxxxxxxx 000 xxxxx 1100111 */
    case 0b1101111:
        return exec_JAL(cpu, inst); /* JAL             xxxxxxx xxxxxxxxxx xxx xxxxx 1101111 */
    case 0b1110011:
        return exec_ECALL_EBREAK(cpu, inst); /* ECALL  0000000 0000000000 000 00000 1110011 */
                                             /* EBREAK 0000000 0000100000 000 00000 1110011 */
    default: return exec_invalid(cpu, inst);
    }
    return 1;
}
