#include "DeadCode.hpp"
#include "Instruction.hpp"
#include "logging.hpp"
#include <memory>
#include <vector>


// 处理流程：两趟处理，mark 标记有用变量，sweep 删除无用指令
void DeadCode::run() {
    bool changed{};
    func_info->run();
    do {
        changed = false;
        for (auto &F : m_->get_functions()) {
            auto func = &F;
            changed |= clear_basic_blocks(func);
            mark(func);
            changed |= sweep(func);
        }
    } while (changed);
    LOG_INFO << "dead code pass erased " << ins_count << " instructions";
}

bool DeadCode::clear_basic_blocks(Function *func) {
    bool changed = 0;
    std::vector<BasicBlock *> to_erase;
    for (auto &bb1 : func->get_basic_blocks()) {
        auto bb = &bb1;
        if(bb->get_pre_basic_blocks().empty() && bb != func->get_entry_block()) {
            to_erase.push_back(bb);
            changed = 1;
        }
    }
    for (auto &bb : to_erase) {
        bb->erase_from_parent();
        delete bb;
    }
    return changed;
}

void DeadCode::mark(Function *func) {
    // 初始化work_list，加入所有critical指令
    work_list.clear();
    marked.clear();
    
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            if (is_critical(&inst)) {
                work_list.push_back(&inst);
                marked[&inst] = true;
            }
        }
    }
    
    // Work list算法，处理所有标记为true的指令的操作数
    while (!work_list.empty()) {
        auto inst = work_list.front();
        work_list.pop_front();
        
        // 标记该指令的所有操作数（如果是指令的话）
        for (auto op : inst->get_operands()) {
            if (auto op_inst = dynamic_cast<Instruction *>(op)) {
                auto it = marked.find(op_inst);
                if (it == marked.end() || !it->second) {
                    marked[op_inst] = true;
                    work_list.push_back(op_inst);
                }
            }
        }
    }
}

void DeadCode::mark(Instruction *ins) {
    // 这个函数可以用来单独标记一个指令，但目前在主算法中未使用
    // 保留为空实现
}

bool DeadCode::sweep(Function *func) {
    // TODO: 删除无用指令
    // 提示：
    // 1. 遍历函数的基本块，删除所有标记为true的指令
    // 2. 删除指令后，可能会导致其他指令的操作数变为无用，因此需要再次遍历函数的基本块
    // 3. 如果删除了指令，返回true，否则返回false
    // 4. 注意：删除指令时，需要先删除操作数的引用，然后再删除指令本身
    // 5. 删除指令时，需要注意指令的顺序，不能删除正在遍历的指令
    std::unordered_set<Instruction *> wait_del{};

    // 1. 收集所有未被标记的指令
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            auto it = marked.find(&inst);
            // 如果在marked中找不到，或者找到了但值为false，则标记为待删除
            if (it == marked.end() || !it->second) {
                wait_del.insert(&inst);
            }
        }
    }

    // 2. 执行删除
    for (auto inst : wait_del) {
        // 先从基本块中删除指令
        inst->get_parent()->erase_instr(inst);
        
        // 不要delete，让系统管理内存
        // delete inst;
        ins_count++;
    }
    
    return not wait_del.empty(); // changed
}

bool DeadCode::is_critical(Instruction *ins) {
    // 终止指令是critical的
    if (ins->isTerminator()) {
        return true;
    }
    
    // 存储指令是critical的
    if (ins->is_store()) {
        return true;
    }
    
    // 有返回值的调用指令中，只有纯函数调用可以被删除
    if (ins->is_call()) {
        auto call_inst = dynamic_cast<CallInst *>(ins);
        if (call_inst && call_inst->func_) {
            // 检查函数是否有效且在module中
            bool func_found = false;
            for (auto &f : m_->get_functions()) {
                if (&f == call_inst->func_) {
                    func_found = true;
                    break;
                }
            }
            
            if (func_found && func_info->is_pure_function(call_inst->func_)) {
                // 纯函数调用且返回值无用可以删除
                return false;
            }
        }
        // 非纯函数调用是critical的
        return true;
    }
    
    // 如果指令有使用，则是critical的
    if (!ins->get_use_list().empty()) {
        return true;
    }
    
    // 其他情况不是critical的
    return false;
}

void DeadCode::sweep_globally() {
    std::vector<Function *> unused_funcs;
    std::vector<GlobalVariable *> unused_globals;
    for (auto &f_r : m_->get_functions()) {
        if (f_r.get_use_list().size() == 0 and f_r.get_name() != "main")
            unused_funcs.push_back(&f_r);
    }
    for (auto &glob_var_r : m_->get_global_variable()) {
        if (glob_var_r.get_use_list().size() == 0)
            unused_globals.push_back(&glob_var_r);
    }
    // changed |= unused_funcs.size() or unused_globals.size();
    for (auto func : unused_funcs)
        m_->get_functions().erase(func);
    for (auto glob : unused_globals)
        m_->get_global_variable().erase(glob);
}
