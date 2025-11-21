#include "ConstPropagation.hpp"

#include "Instruction.hpp"
#include "logging.hpp"

ConstantInt *ConstFolder::compute(Instruction::OpID op, ConstantInt *value1, ConstantInt *value2) {
    int c_value1 = value1->get_value();
    int c_value2 = value2->get_value();

    switch (op) {
    case Instruction::add:
        return ConstantInt::get(c_value1 + c_value2, module_);
        break;
    case Instruction::sub:
        return ConstantInt::get(c_value1 - c_value2, module_);
        break;
    case Instruction::mul:
        return ConstantInt::get(c_value1 * c_value2, module_);
        break;
    case Instruction::sdiv:
        return ConstantInt::get(static_cast<int>(c_value1 / c_value2), module_);
        break;
    case Instruction::eq:
        return ConstantInt::get(c_value1 == c_value2, module_);
        break;
    case Instruction::ne:
        return ConstantInt::get(c_value1 != c_value2, module_);
        break;
    case Instruction::gt:
        return ConstantInt::get(c_value1 > c_value2, module_);
        break;
    case Instruction::ge:
        return ConstantInt::get(c_value1 >= c_value2, module_);
        break;
    case Instruction::lt:
        return ConstantInt::get(c_value1 < c_value2, module_);
        break;
    case Instruction::le:
        return ConstantInt::get(c_value1 <= c_value2, module_);
        break;
    default:
        return nullptr;
        break;
    }
}

ConstantFP *ConstFolder::compute(Instruction::OpID op, ConstantFP *value1, ConstantFP *value2) {
    float c_value1 = value1->get_value();
    float c_value2 = value2->get_value();
    switch (op) {
    case Instruction::fadd:
        return ConstantFP::get(c_value1 + c_value2, module_);
        break;
    case Instruction::fsub:
        return ConstantFP::get(c_value1 - c_value2, module_);
        break;
    case Instruction::fmul:
        return ConstantFP::get(c_value1 * c_value2, module_);
        break;
    case Instruction::fdiv:
        return ConstantFP::get(c_value1 / c_value2, module_);
        break;
    default:
        return nullptr;
        break;
    }
}
ConstantFP *ConstFolder::compute(Instruction::OpID op, ConstantInt *value1) {
    int c_value1 = value1->get_value();

    switch (op) {
    case Instruction::sitofp:
        return ConstantFP::get((float) c_value1, module_);
        break;

    default:
        return nullptr;
        break;
    }
}

ConstantInt *ConstFolder::compute(Instruction::OpID op, ConstantFP *value1) {
    float c_value1 = value1->get_value();
    switch (op) {
    case Instruction::fptosi:
        return ConstantInt::get(static_cast<int>(c_value1), module_);
        break;

    default:
        return nullptr;
        break;
    }
}

ConstantFP *cast_constantfp(Value *value) {
    auto constant_fp_ptr = dynamic_cast<ConstantFP *>(value);
    if (constant_fp_ptr) {
        return constant_fp_ptr;
    }
    return nullptr;
}
ConstantInt *cast_constantint(Value *value) {
    auto constant_int_ptr = dynamic_cast<ConstantInt *>(value);
    if (constant_int_ptr) {
        return constant_int_ptr;
    }
    return nullptr;
}

void ConstPropagation::run() {
    for (auto &func : m_->get_functions()) {

        for (auto &bb : func.get_basic_blocks()) {
            wait_delete.clear();

            for (auto &instr : bb.get_instructions()) {
                // clear glbalvar_def map

                if (instr.is_add() || instr.is_sub() || instr.is_mul() || instr.is_div()) {
                    auto value1 = cast_constantint(instr.get_operand(0));
                    auto value2 = cast_constantint(instr.get_operand(1));
                    if (value1 && value2) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1, value2);

                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                } 
                // TODO: fold other type of expression
                // 处理浮点数操作
                else if (instr.is_fadd() || instr.is_fsub() || instr.is_fmul() || instr.is_fdiv()) {
                    auto value1 = cast_constantfp(instr.get_operand(0));
                    auto value2 = cast_constantfp(instr.get_operand(1));
                    if (value1 && value2) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1, value2);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理整数比较操作
                else if (instr.is_cmp()) {
                    auto value1 = cast_constantint(instr.get_operand(0));
                    auto value2 = cast_constantint(instr.get_operand(1));
                    if (value1 && value2) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1, value2);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理浮点比较 - 这些应该返回i1类型的常数
                else if (instr.is_fcmp()) {
                    auto value1 = cast_constantfp(instr.get_operand(0));
                    auto value2 = cast_constantfp(instr.get_operand(1));
                    if (value1 && value2) {
                        // 浮点比较的结果是布尔值，需要创建ConstantInt
                        float c_value1 = value1->get_value();
                        float c_value2 = value2->get_value();
                        bool result = false;
                        switch (instr.get_instr_type()) {
                        case Instruction::feq:
                            result = c_value1 == c_value2;
                            break;
                        case Instruction::fne:
                            result = c_value1 != c_value2;
                            break;
                        case Instruction::fgt:
                            result = c_value1 > c_value2;
                            break;
                        case Instruction::fge:
                            result = c_value1 >= c_value2;
                            break;
                        case Instruction::flt:
                            result = c_value1 < c_value2;
                            break;
                        case Instruction::fle:
                            result = c_value1 <= c_value2;
                            break;
                        default:
                            break;
                        }
                        auto fold_const = ConstantInt::get(result, m_);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理类型转换
                else if (instr.is_si2fp()) {
                    auto value1 = cast_constantint(instr.get_operand(0));
                    if (value1) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                else if (instr.is_fp2si()) {
                    auto value1 = cast_constantfp(instr.get_operand(0));
                    if (value1) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
            }
            globalvar_def.clear();
            for (auto instr : wait_delete) {
                bb.erase_instr(instr);
            }
        }
    }

    for (auto &func : m_->get_functions()) {
        for (auto &bb : func.get_basic_blocks()) {
            builder->set_insert_point(&bb);
            // TODO: check if conditional branch's condition is constant
            // 检查分支指令的条件是否为常数
            for (auto &instr : bb.get_instructions()) {
                if (instr.is_br()) {
                    auto br_inst = dynamic_cast<BranchInst *>(&instr);
                    if (br_inst && br_inst->is_cond_br()) {
                        auto cond = br_inst->get_condition();
                        if (auto const_val = cast_constantint(cond)) {
                            // 如果条件是常数，则只跳转到一个分支
                            auto true_bb = br_inst->get_operand(1);
                            auto false_bb = br_inst->get_operand(2);
                            if (const_val->get_value() != 0) {
                                // 跳转到true分支，标记false分支为删除
                                delete_bb.push_back(dynamic_cast<BasicBlock *>(false_bb));
                            } else {
                                // 跳转到false分支，标记true分支为删除
                                delete_bb.push_back(dynamic_cast<BasicBlock *>(true_bb));
                            }
                        }
                    }
                }
            }
        }
        for (auto bb : delete_bb) {
            clear_blocks_recs(bb);
        }
        delete_bb.clear();
    }
}

bool ConstPropagation::is_entry(BasicBlock *bb) {
    // TODO
    // 检查一个基本块是否是函数的入口块
    if (bb == nullptr) {
        return false;
    }
    auto func = bb->get_parent();
    if (func == nullptr) {
        return false;
    }
    return bb == func->get_entry_block();
}

void ConstPropagation::clear_blocks_recs(BasicBlock *start_bb) {
    auto func = start_bb->get_parent();
    if (func == nullptr) {
        LOG(ERROR) << "basic block-" << start_bb->get_name() << " has no parent function";
    } else {
        auto prev_bb = start_bb->get_pre_basic_blocks();
        // start_bb has no previous bb and is not the entry of parent function
        if (prev_bb.size() == 0 && !is_entry(start_bb)) {
            func->remove(start_bb);
            auto succ_bb = start_bb->get_succ_basic_blocks();
            for (auto each_succ_bb : succ_bb) {
                std::vector<Instruction*> del_inst;
                for (auto &instr1 : each_succ_bb->get_instructions()) {
                    auto instr = &instr1;
                    if (instr->is_phi()) {
                        LOG(DEBUG) << "Find a PHI instruction in the sucess node of "
                                      "useless branch";
                        for (int i = 1; i < instr->get_num_operand(); i += 2) {
                            if (instr->get_operand(i) == start_bb &&
                                start_bb->get_pre_basic_blocks().size() <= 0) {
                                LOG(DEBUG) << "remove unuseful phi branch in the index of " << i - 1
                                           << " and " << i;

                                instr->remove_operand(i - 1);
                                instr->remove_operand(i - 1);
                            }
                        }
                        int operands_num_phi = instr->get_num_operand();
                        if (operands_num_phi == 2) {
                            auto value = instr->get_operand(0);
                            instr->replace_all_use_with(cast_constantint(value));
                            del_inst.push_back(instr);
                        }
                    }
                }
                for(auto instr : del_inst) each_succ_bb->erase_instr(instr);
                clear_blocks_recs(each_succ_bb);
            }
        }
    }
}
