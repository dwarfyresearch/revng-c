;
; Copyright (c) rev.ng Labs Srl. See LICENSE.md for details.
;

CHECK: define i64 @local_raw_primitives_on_registers(i64 %[[ARG1:.*]], i64 %[[ARG2:.*]]) [[IGN:.*]] {
CHECK-DAG: add i64 [[IGN:.*]]%[[ARG1]]
CHECK-DAG: add i64 [[IGN:.*]]%[[ARG2]]
CHECK: }

CHECK: define i64 @local_raw_pointers_on_registers(i64 %[[ARG1:.*]], i64 %[[ARG2:.*]]) [[IGN:.*]] {
CHECK-DAG: %[[ARG1_PTR:.*]] = inttoptr i64 %[[ARG1]] to i64*
CHECK-DAG: load i64, i64* %[[ARG1_PTR:.*]]
CHECK-DAG: %[[ARG2_PTR:.*]] = inttoptr i64 %[[ARG2]] to i64*
CHECK-DAG: load i64, i64* %[[ARG2_PTR]]
CHECK: }

CHECK: define i64 @local_raw_primitives_on_stack(i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[STACK_ARG:.*]]) [[IGN:.*]] {
CHECK-DAG:   %[[STACK_ARG8:.*]] = add i64 %[[STACK_ARG]], 8
CHECK-DAG:   %[[STACK_ARG8_PTR:.*]] = inttoptr i64 %[[STACK_ARG8]] to i64*
CHECK-DAG:   load i64, i64* %[[STACK_ARG8_PTR:.*]]
CHECK-DAG:   %[[STACK_ARG_PTR:.*]] = inttoptr i64 %[[STACK_ARG]] to i64*
CHECK-DAG:   load i64, i64* %[[STACK_ARG_PTR]]
CHECK: }

CHECK: define i64 @local_cabi_primitives_on_registers(i64 %[[ARG1:.*]], i64 %[[ARG2:.*]]) [[IGN:.*]] {
CHECK-DAG: add i64 [[IGN:.*]][[ARG1]]
CHECK-DAG: add i64 [[IGN:.*]][[ARG2]]
CHECK: }

CHECK: define i64 @local_cabi_primitives_on_stack(i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[STACK_ARG1:.*]], i64 %[[STACK_ARG2:.*]]) [[IGN:.*]] {
CHECK-DAG:   %[[STACK_ARG1_PTR:.*]] = inttoptr i64 %[[STACK_ARG1]] to i64*
CHECK-DAG:   load i64, i64* %[[STACK_ARG1_PTR]]
CHECK-DAG:   %[[STACK_ARG2_PTR:.*]] = inttoptr i64 %[[STACK_ARG2]] to i64*
CHECK-DAG:   load i64, i64* %[[STACK_ARG2_PTR]]
CHECK: }

CHECK: define i64 @local_cabi_aggregate_on_registers(i64 %[[ARG1:.*]]) [[IGN:.*]] {
CHECK-DAG:   %[[FIELD1_PTR:.*]] = inttoptr i64 %[[ARG1]] to i64*
CHECK-DAG:   load i64, i64* %[[FIELD1_PTR]]
CHECK-DAG:   %[[FIELD2_ADDR:.*]] = add i64 %[[ARG1]], 8
CHECK-DAG:   %[[FIELD2_PTR:.*]] = inttoptr i64 %[[FIELD2_ADDR]] to i64*
CHECK-DAG:   load i64, i64* %[[FIELD2_PTR]], align 8
CHECK: }

CHECK: define i64 @local_cabi_aggregate_on_stack(i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[STACK_ARG:.*]]) [[IGN:.*]] {
CHECK-DAG:   %[[FIELD1_PTR:.*]] = inttoptr i64 %[[STACK_ARG]] to i64*
CHECK-DAG:   load i64, i64* %[[FIELD1_PTR]]
CHECK-DAG:   %[[FIELD2_ADDR:.*]] = add i64 %[[STACK_ARG]], 8
CHECK-DAG:   %[[FIELD2_PTR:.*]] = inttoptr i64 %[[FIELD2_ADDR]] to i64*
CHECK-DAG:   load i64, i64* %[[FIELD2_PTR]]
CHECK: }

CHECK: define i64 @local_cabi_aggregate_on_stack_and_registers(i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[IGN:.*]], i64 %[[STACK_ARG:.*]]) [[IGN:.*]] {
CHECK-DAG:   %[[FIELD1_PTR:.*]] = inttoptr i64 %[[STACK_ARG]] to i64*
CHECK-DAG:   load i64, i64* %[[FIELD1_PTR]]
CHECK-DAG:   %[[FIELD2_ADDR:.*]] = add i64 %[[STACK_ARG]], 8
CHECK-DAG:   %[[FIELD2_PTR:.*]] = inttoptr i64 %[[FIELD2_ADDR]] to i64*
CHECK-DAG:   load i64, i64* %[[FIELD2_PTR]]
CHECK: }

CHECK: define i64 @local_caller() [[IGN:.*]] {
CHECK:   = call i64 @local_raw_primitives_on_registers(i64 2, i64 1)
CHECK:   = call i64 @local_raw_pointers_on_registers(i64 %[[ARG:.*]], i64 %[[ARG]])
CHECK:   %[[STACK1:.*]] = call i64 @revng_call_stack_arguments(i64 16)
CHECK:   %[[STACK:.*]] = call i64 @AddressOf([[IGN:.*]], i64 %[[STACK1]])
CHECK:   = call i64 @local_raw_primitives_on_stack(i64 4, i64 3, i64 2, i64 1, i64 5, i64 6, i64 %[[STACK]])
CHECK:   = call i64 @local_cabi_primitives_on_registers(i64 1, i64 2)
TODO: devise a pipeline that highlights both arguments as immediates
CHECK:   = call i64 @local_cabi_primitives_on_stack(i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 [[SCALAR1:.*]], i64 [[SCALAR2:.*]])
CHECK:   %[[AGGREGATE1:.*]] = call i64 @revng_call_stack_arguments(i64 16)
CHECK:   %[[AGGREGATE:.*]] = call i64 @AddressOf([[IGN:.*]], i64 %[[AGGREGATE1]])
CHECK:   = call i64 @local_cabi_aggregate_on_registers(i64 %[[AGGREGATE]])
CHECK:   %[[AGGREGATE1:.*]] = call i64 @revng_call_stack_arguments(i64 16)
CHECK:   %[[AGGREGATE:.*]] = call i64 @AddressOf([[IGN:.*]], i64 %[[AGGREGATE1]])
CHECK:   = call i64 @local_cabi_aggregate_on_stack(i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 %[[AGGREGATE]])
CHECK:   %[[AGGREGATE1:.*]] = call i64 @revng_call_stack_arguments(i64 16)
CHECK:   %[[AGGREGATE:.*]] = call i64 @AddressOf([[IGN:.*]], i64 %[[AGGREGATE1]])
CHECK:   = call i64 @local_cabi_aggregate_on_stack_and_registers(i64 1, i64 2, i64 3, i64 4, i64 5, i64 %[[AGGREGATE]])
CHECK: }