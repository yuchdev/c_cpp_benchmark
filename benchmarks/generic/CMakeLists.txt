cmake_minimum_required(VERSION 3.16)
project(generic C CXX)

add_executable(a_qsort_c a_qsort_c.c)
add_executable(a_std_sort_cpp a_std_sort_cpp.cpp)

add_executable(b_callback_c b_callback_c.c)
add_executable(b_template_cpp b_template_cpp.cpp)

add_executable(c_struct_api c_struct_api.c)
add_executable(c_class_operator_cpp c_class_operator.cpp)

add_executable(d_buffer_copy_c d_buffer_copy_c.c)
add_executable(d_buffer_move_cpp d_buffer_move_cpp.cpp)

add_executable(e_runtime_table_c e_runtime_table_c.c)
add_executable(e_constexpr_table_cpp e_constexpr_table_cpp.cpp)
