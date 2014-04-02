#ifndef DAGGER_DC_DCINSTRSEMA_H
#define DAGGER_DC_DCINSTRSEMA_H

#include "llvm/DC/DCOpcodes.h"
#include "llvm/DC/DCRegisterSema.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/MC/MCAnalysis/MCAtom.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/ErrorHandling.h"
#include <vector>

namespace llvm {
class MCContext;
}

namespace llvm {
class DCTranslatedInst;

class DCInstrSema {
public:
  virtual ~DCInstrSema();

  bool translateInst(const MCDecodedInst &DecodedInst,
                     DCTranslatedInst &TranslatedInst);

  void SwitchToModule(Module *TheModule);
  void SwitchToFunction(uint64_t StartAddress);
  void SwitchToBasicBlock(uint64_t StartAddress, uint64_t EndAddress);

  void FinalizeModule();
  Function *FinalizeFunction();
  void FinalizeBasicBlock();

  BasicBlock *getOrCreateBasicBlock(uint64_t StartAddress);

  Function *createMainFunction(Function *EntryFn);
  Function *getInitRegSetFunction() { return InitFn; }
  Function *getFiniRegSetFunction() { return FiniFn; }

  void createExternalWrapperFunction(uint64_t Addr, StringRef Name);
  void createExternalTailCallBB(uint64_t Addr);

        DCRegisterSema &getDRS()       { return DRS; }
  const DCRegisterSema &getDRS() const { return DRS; }

private:
  // Autogenerated by tblgen
  const unsigned *OpcodeToSemaIdx;
  const unsigned *SemanticsArray;
  const uint64_t *ConstantArray;

protected:
  DCInstrSema(const unsigned *OpcodeToSemaIdx, const unsigned *SemanticsArray,
              const uint64_t *ConstantArray, DCRegisterSema &DRS);

  LLVMContext *Ctx;
  Module *TheModule;
  DCRegisterSema &DRS;
  FunctionType *FuncType;
  Function *InitFn;
  Function *FiniFn;

  // Following members are valid only inside a Function
  Function *TheFunction;
  std::map<uint64_t, BasicBlock *> BBByAddr;
  BasicBlock *ExitBB;
  std::vector<BasicBlock *> CallBBs;

  // Following members are valid only inside a Basic Block
  BasicBlock *TheBB;
  // FIXME: This should be integrated with MCAtoms
  uint64_t BBStartAddr;
  uint64_t BBEndAddr;
  typedef IRBuilder<true, NoFolder> DCIRBuilder;
  std::unique_ptr<DCIRBuilder> Builder;

  // translation vars.
  unsigned Idx;
  EVT ResEVT;
  unsigned Opcode;
  SmallVector<Value *, 16> Vals;
  const MCDecodedInst *CurrentInst;

  unsigned Next() { return SemanticsArray[Idx++]; }
  EVT NextVT() { return EVT(MVT::SimpleValueType(Next())); }

  uint64_t getImmOp(unsigned Idx) {
    return CurrentInst->Inst.getOperand(Idx).getImm();
  }
  unsigned getRegOp(unsigned Idx) {
    return CurrentInst->Inst.getOperand(Idx).getReg();
  }

  Value *getReg(unsigned RegNo) { return DRS.getReg(RegNo); }
  void setReg(unsigned RegNo, Value *Val) { DRS.setReg(RegNo, Val); }

  Function *getFunction(uint64_t Addr);

  void insertCall(Value *CallTarget);
  Value *insertTranslateAt(Value *OrigTarget);

  virtual void translateTargetOpcode() = 0;
  virtual void translateCustomOperand(unsigned OperandType,
                                      unsigned MIOperandNo) = 0;
  virtual void translateImplicit(unsigned RegNo) = 0;

  // Try to do a custom translation of a full instruction.
  // Called before translating an instruction.
  // Return true if the translation shouldn't proceed.
  virtual bool translateTargetInst() { return false; }

private:
  void translateOperand(unsigned OperandType, unsigned MIOperandNo);

  void translateBinOp(Instruction::BinaryOps Opc);
  void translateCastOp(Instruction::CastOps Opc);

  BasicBlock *insertCallBB(Value *CallTarget);

  void removeTrapInstFromEmptyBB(BasicBlock *BB);
};

DCInstrSema *createDCInstrSema(StringRef Triple, const MCRegisterInfo &MRI,
                               const MCInstrInfo &MII);

} // end namespace llvm

#endif
