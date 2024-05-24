/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "cyber/croutine/detail/routine_context.h"

namespace apollo {
namespace cyber {
namespace croutine {

//  The stack layout looks as follows:
//
//                ^  +------------------+  addr大
//                |  |      Reserved    |  协程栈顶,Reserved一个空间,避免踩内存?
//                |  +------------------+
//                |  |  Return Address  |   f1 -> void CRoutineEntry(void *arg)
//                |  +------------------+
//                |  |        RDI       |   arg: this -> CRoutine obj
//                |  +------------------+
//                /  |        R12       |
// STACK_SIZE <---   +------------------+
//                \  |        R13       |
//                |  +------------------+
//                |  |        ...       |
//                |  +------------------+
// ctx->sp  =>    |  |        RBP       |  协程栈底(线程复用sp,函数堆栈向下增长)
//                |  +------------------+
//                |  |        ...       |
//                |  +------------------+
// ctx->stack=>   |  |                  |  RoutineContext起始地址
//                V  +------------------+  addr小
// mark: 把函数地址 f1 和函数参数 arg 保存到协程上下文 ctx
//       https://blog.csdn.net/jinzhuojun/article/details/86760743
void MakeContext(const func &f1, const void *arg, RoutineContext *ctx) {
  // mark: 只利用了预分配的2M内存的栈顶一部分内存
  ctx->sp = ctx->stack + STACK_SIZE - 2 * sizeof(void *) - REGISTERS_SIZE;
  std::memset(ctx->sp, 0, REGISTERS_SIZE);
#ifdef __aarch64__
  char *sp = ctx->stack + STACK_SIZE - sizeof(void *);
#else
  char *sp = ctx->stack + STACK_SIZE - 2 * sizeof(void *);
#endif
  // mark: ctx 保存 f1 函数地址
  *reinterpret_cast<void **>(sp) = reinterpret_cast<void *>(f1);
  sp -= sizeof(void *);
  // mark: ctx 保存 f1 函数入参
  *reinterpret_cast<void **>(sp) = const_cast<void *>(arg);
}

}  // namespace croutine
}  // namespace cyber
}  // namespace apollo
