# 简介

本仓库为 USTC 编译工程 2025 的课程实验仓库。在本学期的编译实验中，你们将构建一个从词法分析器开始到后端代码生成的JIANMU编译器。

你们需要 fork 此 repo 到自己的仓库下，随后在自己的仓库中完成实验。


## 测试脚本使用方法

eval_lab2.sh: 
    没有参数，直接运行即可，结果会生成在 eval_result 下

如何编译：
``` bash
# 如果你想安装到usr/local/bin
cmake -B build && cmake --install build
# 否则 执行以下指令
cmake -B build && cmake --build build
```

无论哪种命令，你都可以在 build/ 下找到 cminusfc 等可执行文件


lab2 eval_result:
```txt
Running with optimizations: -dce -func-inline -const-prop

===========lv0_1 START========
return:	Success
decl_int:	Success
decl_float:	Success
decl_int_array:	Success
decl_float_array:	Success
input:	Success
output_float:	Success
output_int:	Success
points of lv0_1 is: 17
===========lv0_1 END========

===========lv0_2 START========
num_add_int:	Success
num_sub_int:	Success
num_mul_int:	Success
num_div_int:	Success
num_add_float:	Success
num_sub_float:	Success
num_mul_float:	Success
num_div_float:	Success
num_add_mixed:	Success
num_sub_mixed:	Success
num_mul_mixed:	Success
num_div_mixed:	Success
num_comp1:	Success
num_le_int:	Success
num_lt_int:	Success
num_ge_int:	Success
num_gt_int:	Success
num_eq_int:	Success
num_neq_int:	Success
num_le_float:	Success
num_lt_float:	Success
num_ge_float:	Success
num_gt_float:	Success
num_eq_float:	Success
num_neq_float:	Success
num_le_mixed:	Success
num_lt_mixed:	Success
num_ge_mixed:	Success
num_gt_mixed:	Success
num_eq_mixed:	Success
num_neq_mixed:	Success
num_comp2:	Success
points of lv0_2 is: 18
===========lv0_2 END========

===========lv1 START========
assign_int_var_local:	Success
assign_int_array_local:	Success
assign_int_var_global:	Success
assign_int_array_global:	Success
assign_float_var_local:	Success
assign_float_array_local:	Success
assign_float_var_global:	Success
assign_float_array_global:	Success
assign_cmp:	Success
innout:	Success
idx_float:	Success
negidx_int:	Success
negidx_float:	Success
negidx_intfuncall:	Success
negidx_floatfuncall:	Success
negidx_voidfuncall:	Success
selection1:	Success
selection2:	Success
selection3:	Success
iteration1:	Success
iteration2:	Success
scope:	Success
transfer_float_to_int:	Success
transfer_int_to_float:	Success
points of lv1 is: 31
===========lv1 END========

===========lv2 START========
funcall_chain:	Success
assign_chain:	Success
funcall_var:	Success
funcall_int_array:	Success
funcall_float_array:	Success
funcall_array_array:	Success
return_in_middle1:	Success
return_in_middle2:	Success
funcall_type_mismatch1:	Success
funcall_type_mismatch2:	Success
return_type_mismatch1:	Success
return_type_mismatch2:	Success
points of lv2 is: 23
===========lv2 END========

===========lv3 START========
complex1:	Success
complex2:	Success
complex3:	Success
points of lv3 is: 11
===========lv3 END========

total points: 100

```