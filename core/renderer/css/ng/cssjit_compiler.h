// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef CORE_RENDERER_CSS_NG_CSSJIT_COMPILER_H_
#define CORE_RENDERER_CSS_NG_CSSJIT_COMPILER_H_

#include <string>

// cspell:disable
namespace asmjit {
class JitRuntime;
}

namespace lynx {
namespace css {
class LynxCSSSelector;
class MacroAssembler;
class StyleNode;

using JitCodeMatcher = int (*)(StyleNode *);

class CSSJITCompiler {
 public:
  CSSJITCompiler();
  ~CSSJITCompiler();
  CSSJITCompiler(const CSSJITCompiler &) = delete;
  CSSJITCompiler &operator=(const CSSJITCompiler &) = delete;

  JitCodeMatcher Compile(const LynxCSSSelector *);

  static void CompileOnBackgroundThread(LynxCSSSelector *);
  static void WaitBackgroundThreadIdle();

 private:
  asmjit::JitRuntime *rt_;
  MacroAssembler *__;

  bool MatchSelector(const LynxCSSSelector *);
  bool MatchSimple(const LynxCSSSelector *);
  bool MatchForRelation(const LynxCSSSelector *, int);
  bool MatchPseudoClass(const LynxCSSSelector *);
  bool MatchPseudoElement(const LynxCSSSelector *);
  bool MatchPseudoNot(const LynxCSSSelector *);
  void MatchPseudoState(const LynxCSSSelector *, uint32_t state);
  void MatchString(const std::string &str, void *fn);
};
}  // namespace css
}  // namespace lynx

#endif  // CORE_RENDERER_CSS_NG_CSSJIT_COMPILER_H_
