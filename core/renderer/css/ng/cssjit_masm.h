// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef CORE_RENDERER_CSS_NG_CSSJIT_MASM_H_
#define CORE_RENDERER_CSS_NG_CSSJIT_MASM_H_

#include <stdio.h>

// cspell:disable
#if defined(_M_X64) || defined(__x86_64__)
#include "third_party/asmjit/asmjit/x86.h"
#define arch asmjit::x86
#define kReturnValueReg arch::rax
#define kElementReg arch::rbx
#ifdef _WIN32
#define kArg1Reg arch::rcx
#define kArg2Reg arch::rdx
#define kArg3Reg arch::r8
#define kArg4Reg arch::r9
#else
#define kArg1Reg arch::rdi
#define kArg2Reg arch::rsi
#define kArg3Reg arch::rdx
#define kArg4Reg arch::rcx
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#include "third_party/asmjit/asmjit/a64.h"
#define arch asmjit::a64
#define kReturnValueReg arch::x0
#define kElementReg arch::x19
#else
#error unsupported architecture
#endif

namespace lynx {
namespace css {

class MacroAssembler : public arch::Assembler {
 public:
  MacroAssembler(asmjit::JitRuntime* rt) : rt_(rt) {
    code_.init(rt->environment(), rt->cpu_features());
    code_.attach(this);
#ifndef ASMJIT_NO_LOGGING
    logger_.set_indentation(asmjit::FormatIndentationGroup::kCode, 6);
    logger_.set_indentation(asmjit::FormatIndentationGroup::kComment, 6);
    code_.set_logger(&logger_);
#endif
  }

  void Comment(const char* data, bool inline_comment = true) {
#ifndef ASMJIT_NO_LOGGING
    if (inline_comment) {
      set_inline_comment(data);
    } else {
      comment(data);
    }
#endif
  }

  asmjit::Label Label(const char* name) {
#ifndef ASMJIT_NO_LOGGING
    if (code_.label_id_by_name(name) == asmjit::Globals::kInvalidId) {
      return new_named_label(name);
    }
    char ext_name[64];
    for (int ext = 2; ext < 100; ext++) {
      snprintf(ext_name, sizeof(ext_name), "%s%d", name, ext);
      if (code_.label_id_by_name(ext_name) == asmjit::Globals::kInvalidId) {
        return new_named_label(ext_name);
      }
    }
    return new_label();
#else
    return new_label();
#endif
  }

  JitCodeMatcher Generate() {
    JitCodeMatcher fn;
    auto err = rt_->add(&fn, &code_);
    if (err != asmjit::Error::kOk) {
      return nullptr;
    }
    return fn;
  }

  void Prolog() {
#if defined(_M_X64) || defined(__x86_64__)
    push(kElementReg);
    mov(kElementReg, kArg1Reg);
#else
    stp(arch::x29, arch::x30, arch::ptr_pre(arch::sp, -16));
    str(kElementReg, arch::ptr_pre(arch::sp, -16));
    mov(kElementReg, arch::x0);
#endif
  }

  void Epilog() {
#if defined(_M_X64) || defined(__x86_64__)
    pop(kElementReg);
    ret();
#else
    ldr(kElementReg, arch::ptr_post(arch::sp, 16));
    ldp(arch::x29, arch::x30, arch::ptr_post(arch::sp, 16));
    ret(arch::x30);
#endif
  }

  void Push(arch::Gp reg) {
#if defined(_M_X64) || defined(__x86_64__)
    push(reg);
#else
    str(reg, arch::ptr_pre(arch::sp, -16));
#endif
  }
  void Pop(arch::Gp reg) {
#if defined(_M_X64) || defined(__x86_64__)
    pop(reg);
#else
    ldr(reg, arch::ptr_post(arch::sp, 16));
#endif
  }

  template <typename X, typename Y>
  void Mov(X dst, Y src, const char* comment = nullptr) {
    if (comment) {
      Comment(comment);
    }
    mov(dst, src);
  }

  void Goto(asmjit::Label label, const char* comment = nullptr) {
    if (comment) {
      Comment(comment);
    }
#if defined(_M_X64) || defined(__x86_64__)
    jmp(label);
#else
    b(label);
#endif
    Comment("", false);  // newline
  }

  template <typename X, typename Y>
  void GotoIf(asmjit::Label label, X a, Y b, const char* comment = nullptr) {
    if (comment) {
      Comment(comment);
    }
    cmp(a, b);
#if defined(_M_X64) || defined(__x86_64__)
    je(label);
#else
    b_eq(label);
#endif
    Comment("", false);  // newline
  }

  template <typename X, typename Y>
  void GotoIfNot(asmjit::Label label, X a, Y b, const char* comment = nullptr) {
    if (comment) {
      Comment(comment);
    }
    cmp(a, b);
#if defined(_M_X64) || defined(__x86_64__)
    jne(label);
#else
    b_ne(label);
#endif
    Comment("", false);  // newline
  }

  void Call(void* fn, arch::Gp arg1) {
#if defined(_M_X64) || defined(__x86_64__)
    mov(kArg1Reg, arg1);
    call((uint64_t)fn);
#else
    mov(arch::x5, (uint64_t)fn);
    mov(arch::x0, arg1);
    blr(arch::x5);
#endif
  }
  void Call(void* fn, arch::Gp arg1, uint32_t arg2,
            const char* arg2_comment = nullptr) {
#if defined(_M_X64) || defined(__x86_64__)
    mov(kArg1Reg, arg1);
    if (arg2_comment) {
      Comment(arg2_comment);
    }
    mov(kArg2Reg, arg2);
    call((uint64_t)fn);
#else
    mov(arch::x5, (uint64_t)fn);
    mov(arch::x0, arg1);
    if (arg2_comment) {
      Comment(arg2_comment);
    }
    mov(arch::x1.w(), arg2);
    blr(arch::x5);
#endif
  }
  void Call(void* fn, arch::Gp arg1, const void* arg2, uint32_t arg3,
            const char* arg2_comment = nullptr,
            const char* arg3_comment = nullptr) {
#if defined(_M_X64) || defined(__x86_64__)
    mov(kArg1Reg, arg1);
    if (arg2_comment) {
      Comment(arg2_comment);
    }
    mov(kArg2Reg, arg2);
    if (arg3_comment) {
      Comment(arg3_comment);
    }
    mov(kArg3Reg, arg3);
    call((uint64_t)fn);
#else
    mov(arch::x5, (uint64_t)fn);
    mov(arch::x0, arg1);
    if (arg2_comment) {
      Comment(arg2_comment);
    }
    mov(arch::x1.x(), (uint64_t)arg2);
    if (arg3_comment) {
      Comment(arg3_comment);
    }
    mov(arch::x2.w(), arg3);
    blr(arch::x5);
#endif
  }

 private:
  asmjit::JitRuntime* rt_;
  asmjit::CodeHolder code_;
#ifndef ASMJIT_NO_LOGGING
  asmjit::FileLogger logger_{stdout};
#endif
};

}  // namespace css
}  // namespace lynx

#endif  // CORE_RENDERER_CSS_NG_CSSJIT_MASM_H_
