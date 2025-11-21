#include "../../include/passes/FunctionInline.hpp"
#include "../../include/lightir/Function.hpp"

#include "BasicBlock.hpp"
#include "Instruction.hpp"
#include "Value.hpp"
#include "logging.hpp"
#include <cassert>
#include <map>
#include <utility>
#include <vector>

void FunctionInline::run() { inline_all_functions(); }

void FunctionInline::inline_all_functions() {
    
    std::set<Function *> recursive_func;
    for (auto &func : m_->get_functions()) {
        if (func.is_declaration()) {
            continue;
        }
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
        if (func.is_declaration()) {
            continue;
        }
        if (outside_func.find(func.get_name()) != outside_func.end()) {
            continue;
        }
        
        // 反复inline，直到没有可以inline的函数调用为止
        bool changed = true;
        while (changed) {
            changed = false;
            std::vector<std::pair<Instruction*, Function*>> calls_to_inline;
            
            // 收集所有可以inline的调用
            for (auto &bb : func.get_basic_blocks()) {
                for (auto &inst : bb.get_instructions()) {
                    if (inst.is_call()) {
                        auto call = &inst;
                        auto func1 = static_cast<Function *>(call->get_operand(0));
                        
                        // 检查是否应该inline这个调用
                        if (func1 == &func) {
                            continue; // 递归调用，不inline
                        }
                        if (recursive_func.find(func1) != recursive_func.end()) {
                            continue; // 递归函数，不inline
                        }
                        if (outside_func.find(func1->get_name()) != outside_func.end()) {
                            continue; // 外部函数，不inline
                        }
                        if (func1->is_declaration()) {
                            continue;
                        }
                        if (func1->get_basic_blocks().size() >= 6) {
                            continue; // 函数太大，不inline
                        }
                        
                        calls_to_inline.push_back({&inst, func1});
                        changed = true;
                    }
                }
            }
            
            // 逐个inline这些调用
            for (auto [call, called_func] : calls_to_inline) {
                inline_function(call, called_func);
            }
        }
        
        // 在该函数所有inline完成后，重置块信息
        func.reset_bbs();
    }
}

void FunctionInline::inline_function(Instruction *call, Function *origin) {
    if (origin == nullptr || origin->is_declaration()) {
        return;
    }
    std::map<Value *, Value *> v_map;
    std::vector<BasicBlock *> bb_list;
    
    // 创建参数映射：函数参数 -> 调用时的实际参数
    for (auto &arg : origin->get_args()) {
        v_map.insert(std::make_pair(static_cast<Value *>(&arg),
                                    call->get_operand(arg.get_arg_no() + 1)));
    }
    
    auto call_bb = call->get_parent();
    auto call_func = call_bb->get_parent();
    std::vector<BasicBlock *> orig_succs;
    Instruction *old_terminator = nullptr;
    if (call_bb->is_terminated()) {
        old_terminator = call_bb->get_terminator();
        if (old_terminator->is_br()) {
            auto br_inst = static_cast<BranchInst *>(old_terminator);
            if (br_inst->is_cond_br()) {
                orig_succs.push_back(static_cast<BasicBlock *>(br_inst->get_operand(1)));
                orig_succs.push_back(static_cast<BasicBlock *>(br_inst->get_operand(2)));
            } else {
                orig_succs.push_back(static_cast<BasicBlock *>(br_inst->get_operand(0)));
            }
        }
    }
    
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

            // 克隆指令
            Instruction *inst_new = inst.clone(bb_new);
            if (inst.is_phi()) {
                bb_new->add_instruction(inst_new);
            }
            
            // 记录指令映射关系（必须在所有指令后面都做这个）
            v_map.insert(std::make_pair(static_cast<Value *>(&inst),
                                        static_cast<Value *>(inst_new)));
        }
    }
    
    // 步骤2: 收集返回指令和它们所在的块
    std::vector<std::pair<Instruction*, BasicBlock*>> ret_info; // (返回指令，所在块)
    for (auto &bb : origin->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            if (inst.is_ret()) {
                auto ret_bb = static_cast<BasicBlock *>(v_map[static_cast<Value *>(&bb)]);
                ret_info.push_back({&inst, ret_bb});
            }
        }
    }
    
    // 步骤3: 更新操作数映射
    for (auto bb : bb_list) {
        for (auto &inst : bb->get_instructions()) {
            for (unsigned i = 0; i < inst.get_num_operand(); i++) {
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
        if (ret_info.size() == 1) {
            // 单个返回值：直接使用返回值
            auto [ret_inst, ret_bb] = ret_info.front();
            auto mapped_val = ret_inst->get_operand(0);
            if (v_map.count(mapped_val)) {
                mapped_val = v_map[mapped_val];
            }
            ret_val = mapped_val;
            BranchInst::create_br(bb_merge, ret_bb);
        } else if (!ret_info.empty()) {
            // 多个返回值：创建PHI指令进行合并
            auto phi = PhiInst::create_phi(origin->get_return_type(), bb_merge);
            bb_merge->add_instr_begin(phi);
            for (auto &[ret_inst, ret_bb] : ret_info) {
                auto ret_val_i = ret_inst->get_operand(0);
                if (v_map.count(ret_val_i)) {
                    ret_val_i = v_map[ret_val_i];
                }
                phi->add_phi_pair_operand(ret_val_i, ret_bb);
                BranchInst::create_br(bb_merge, ret_bb);
            }
            ret_val = phi;
        }
    } else {
        // void返回类型：直接跳转到合并块
        for (auto &[ret_inst, ret_bb] : ret_info) {
            BranchInst::create_br(bb_merge, ret_bb);
        }
    }
    
    // 步骤6: 处理调用指令所在的基本块
    // 收集call之前和之后的所有指令（使用安全的迭代）
    std::vector<Instruction *> instructions_after_call;
    {
        std::vector<Instruction *> all_insts;
        for (auto &inst : call_bb->get_instructions()) {
            all_insts.push_back(&inst);
        }
        
        for (size_t i = 0; i < all_insts.size(); i++) {
            if (all_insts[i] == call) {
                // 收集call之后的所有指令
                for (size_t j = i + 1; j < all_insts.size(); j++) {
                    instructions_after_call.push_back(all_insts[j]);
                }
                break;
            }
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
    
    // 更新原有后继块中的 phi 前驱信息
    for (auto succ_bb : orig_succs) {
        if (succ_bb == nullptr) {
            continue;
        }
        for (auto &inst : succ_bb->get_instructions()) {
            if (!inst.is_phi()) {
                break;
            }
            auto phi_inst = static_cast<PhiInst *>(&inst);
            for (unsigned idx = 1; idx < phi_inst->get_num_operand(); idx += 2) {
                if (phi_inst->get_operand(idx) == call_bb) {
                    phi_inst->set_operand(idx, bb_merge);
                }
            }
        }
    }
    
    // 重建被内联函数的CFG，避免污染原函数的前驱/后继信息
    origin->reset_bbs();
}