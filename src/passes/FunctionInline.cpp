#include "../../include/passes/FunctionInline.hpp"
#include "../../include/lightir/Function.hpp"

#include "BasicBlock.hpp"
#include "Instruction.hpp"
#include "Value.hpp"
#include "logging.hpp"
#include <cassert>
#include <utility>
#include <vector>

void FunctionInline::run() { inline_all_functions(); }

void FunctionInline::inline_all_functions() {
    
    std::set<Function *> recursive_func;
    for (auto &func : m_->get_functions()) {
        for (auto &bb : func.get_basic_blocks()) {
            for (auto &inst : bb.get_instructions()) {
                if (inst.is_call()) {
                    auto call = &inst;
                    auto func1 = static_cast<Function *>(call->get_operand(0));
                    if (func1 == &func) {
                        recursive_func.insert(func1);
                        break;
                    }
                }
            }
        }
    }
    for (auto &func : m_->get_functions()) {
        if (outside_func.find(func.get_name()) != outside_func.end()) {
            continue;
        }
    a1:
        for (auto &bb : func.get_basic_blocks()) {
            for (auto &inst : bb.get_instructions()) {
                if (inst.is_call()) {
                    auto call = &inst;
                    auto func1 = static_cast<Function *>(call->get_operand(0));
                    if (func1 == &func) {
                        continue;
                    }
                    if (recursive_func.find(func1) != recursive_func.end())
                        continue;
                    if (outside_func.find(func1->get_name()) !=
                        outside_func.end())
                        continue;
                    if(func1->get_basic_blocks().size() >=6){
                        continue;
                    }
                    inline_function(call, func1);
                    goto a1;
                }
            }
        }
    }
}

void FunctionInline::inline_function(Instruction *call, Function *origin) {
    std::map<Value *, Value *> v_map;
    std::vector<BasicBlock *> bb_list;
    std::vector<Instruction *> ret_list; // 记录函数所有出口
    
    // 创建参数映射：函数参数 -> 调用时的实际参数
    for (auto &arg : origin->get_args()) {
        v_map.insert(std::make_pair(static_cast<Value *>(&arg),
                                    call->get_operand(arg.get_arg_no() + 1)));
    }
    
    auto call_bb = call->get_parent();
    auto call_func = call_bb->get_parent();
    
    // 步骤1: 克隆被内联函数的所有基本块
    for (auto &bb : origin->get_basic_blocks()) {
        auto bb_new = BasicBlock::create(call_func->get_parent(), "", call_func);
        v_map.insert(std::make_pair(static_cast<Value *>(&bb),
                                    static_cast<Value *>(bb_new)));
        bb_list.push_back(bb_new);
        
        for (auto &inst : bb.get_instructions()) {
            // 跳过返回语句，在后续处理
            if (inst.is_ret()) {
                continue;
            }
            
            // 跳过PHI指令，在后续处理
            if (inst.is_phi()) {
                continue;
            }
            
            // 克隆指令
            Instruction *inst_new;
            if (inst.is_call()) {
                auto call_inst = static_cast<CallInst *>(&inst);
                auto func = static_cast<Function *>(call_inst->get_operand(0));
                // 创建新的调用指令，跳过第一个操作数（函数指针）
                inst_new = new CallInst(func, {call_inst->get_operands().begin() + 1, call_inst->get_operands().end()}, bb_new);
            } else {
                inst_new = inst.clone(bb_new);
            }
            
            // 记录指令映射关系
            v_map.insert(std::make_pair(static_cast<Value *>(&inst),
                                        static_cast<Value *>(inst_new)));
        }
    }
    
    // 步骤2: 处理返回指令并记录映射
    for (auto &bb : origin->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            if (inst.is_ret()) {
                auto ret_clone = inst.clone(static_cast<BasicBlock *>(v_map[static_cast<Value *>(&bb)]));
                ret_list.push_back(ret_clone);
                v_map.insert(std::make_pair(static_cast<Value *>(&inst),
                                            static_cast<Value *>(ret_clone)));
            }
        }
    }
    
    // 步骤3: 更新操作数映射
    for (auto bb : bb_list) {
        for (auto &inst : bb->get_instructions()) {
            for (int i = 0; i < inst.get_num_operand(); i++) {
                auto op = inst.get_operand(i);
                if (v_map.find(op) != v_map.end()) {
                    inst.set_operand(i, v_map[op]);
                }
            }
        }
    }
    
    // 步骤4: 创建合并基本块，用于汇聚返回路径
    Value *ret_val = nullptr;
    BasicBlock *bb_merge = BasicBlock::create(call_func->get_parent(), "", call_func);
    
    if (!origin->get_return_type()->is_void_type()) {
        if (ret_list.size() == 1) {
            // 单个返回值：直接使用返回值
            auto ret = ret_list.front();
            ret_val = ret->get_operand(0);
            auto ret_bb = ret->get_parent();
            ret_bb->remove_instr(ret);
            BranchInst::create_br(bb_merge, ret_bb);
        } else if (ret_list.size() > 1) {
            // 多个返回值：创建PHI指令进行合并
            auto phi = PhiInst::create_phi(origin->get_return_type(), bb_merge);
            bb_merge->add_instr_begin(phi);  // 添加PHI指令到块的开头
            for (auto ret : ret_list) {
                auto ret_val_i = ret->get_operand(0);
                auto ret_bb = ret->get_parent();
                ret_bb->remove_instr(ret);
                phi->add_phi_pair_operand(ret_val_i, ret_bb);
                BranchInst::create_br(bb_merge, ret_bb);
            }
            ret_val = phi;
        }
    } else {
        // void返回类型：直接跳转到合并块
        for (auto ret : ret_list) {
            auto ret_bb = ret->get_parent();
            ret_bb->remove_instr(ret);
            BranchInst::create_br(bb_merge, ret_bb);
        }
    }
    
    // 步骤5: 处理调用指令所在的基本块
    // 收集call指令之前的所有指令和之后的所有指令
    std::vector<Instruction *> instructions_before_call;
    std::vector<Instruction *> instructions_after_call;
    bool found_call = false;
    
    for (auto &inst : call_bb->get_instructions()) {
        if (&inst == call) {
            found_call = true;
            continue;
        }
        if (!found_call) {
            instructions_before_call.push_back(&inst);
        } else {
            instructions_after_call.push_back(&inst);
        }
    }
    
    // 替换call的所有使用
    if (!origin->get_return_type()->is_void_type()) {
        call->replace_all_use_with(ret_val);
    }
    
    // 移除call指令及其之后的所有指令
    call_bb->remove_instr(call);
    for (auto inst : instructions_after_call) {
        call_bb->remove_instr(inst);
    }
    
    // 添加跳转到第一个inlined块（如果call_bb还没有被终止）
    if (!call_bb->is_terminated()) {
        BranchInst::create_br(bb_list.front(), call_bb);
    }
    
    // 将call之后的指令移到合并块
    for (auto inst : instructions_after_call) {
        bb_merge->add_instruction(inst);
        inst->set_parent(bb_merge);
    }
    
    origin->reset_bbs();
    call_func->reset_bbs();
}