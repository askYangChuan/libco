/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/

.globl coctx_swap
#if !defined( __APPLE__ ) && !defined( __FreeBSD__ )
.type  coctx_swap, @function
#endif
coctx_swap:

#if defined(__i386__)
	leal 4(%esp), %eax //sp 
	movl 4(%esp), %esp 
	leal 32(%esp), %esp //parm a : &regs[7] + sizeof(void*)

	pushl %eax //esp ->parm a 

	pushl %ebp
	pushl %esi
	pushl %edi
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl -4(%eax)

	
	movl 4(%eax), %esp //parm b -> &regs[0]

	popl %eax  //ret func addr
	popl %ebx  
	popl %ecx
	popl %edx
	popl %edi
	popl %esi
	popl %ebp
	popl %esp
	pushl %eax //set ret func addr

	xorl %eax, %eax
	ret

#elif defined(__x86_64__)
	leaq 8(%rsp),%rax                       /* 参考函数调用原理，当前%rsp是存的返回地址，+8就是父函数的真正的栈顶指针，存入%rax */
	leaq 112(%rdi),%rsp                     /* 准备保存当前运行状态的寄存器，%rdi是当前运行的协程的ctx地址，偏移reg[14]个位置 */
	pushq %rax                              /* 将各个寄存器的数据存入ctx的reg中,rax存入到reg[13], r15就存到了reg[0]*/
	pushq %rbx
	pushq %rcx
	pushq %rdx

	pushq -8(%rax) //ret func addr          /* rax当前指向了父函数的真正栈顶指针，因此-8就是子函数完成后返回地址，实际上应该就是co_swap的coctx_swap处 */

	pushq %rsi
	pushq %rdi
	pushq %rbp
	pushq %r8
	pushq %r9
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	
	movq %rsi, %rsp                         /* 栈顶指针切换到正要运行的协程，并取出当前协程的reg的数据，分别还原 */
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r9
	popq %r8
	popq %rbp
	popq %rdi
	popq %rsi
	popq %rax //ret func addr               /* reg[9]存的返回地址，赋值给了rax */
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rsp                               /* reg[13]存的是之前保存的rax，即即将运行的协程的父函数的栈顶指针 */
	pushq %rax                              /* 返回地址入栈 */
	
	xorl %eax, %eax                         /* 让rax的低32bit清空 */
	ret                                     /* 切换完成，返回地址出栈并被调用 */
#endif
