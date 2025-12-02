// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "core/renderer/css/ng/cssjit_compiler.h"

#include "base/include/string/string_utils.h"
#include "core/renderer/css/ng/css_ng_utils.h"
#include "core/renderer/css/ng/cssjit_masm.h"
#include "core/renderer/css/ng/matcher/selector_matcher.h"
#include "core/renderer/css/ng/selector/lynx_css_selector_list.h"
#include "core/renderer/css/style_node.h"

// cspell:disable
namespace lynx {
namespace css {

// Assembly stubs
static inline bool streq(const std::string& s, const char* str, uint32_t len) {
  return s.size() == len && memcmp(s.c_str(), str, len) == 0;
}
static int matchtag(StyleNode* node, const char* str, uint32_t len) {
  const auto& s = node->tag().str();
  return streq(s, str, len) ? kMatches : kFailsLocally;
}
static int matchid(StyleNode* node, const char* str, uint32_t len) {
  const auto& s = node->idSelector().str();
  return streq(s, str, len) ? kMatches : kFailsLocally;
}
static int matchcls(StyleNode* node, const char* str, uint32_t len) {
  for (const auto& c : node->classes()) {
    if (streq(c.str(), str, len)) {
      return kMatches;
    }
  }
  return kFailsLocally;
}
static int matchpseudo(StyleNode* node, uint32_t state) {
  return node->HasPseudoState(state) ? kMatches : kFailsLocally;
}
static int isroot(StyleNode* node) {
  const auto& s = node->tag().str();
  return streq(s, "page", 4) ? kMatches : kFailsLocally;
}
static void* getparent(StyleNode* node) {
  return node->SelectorMatchingParent();
}
static void* getpsib(StyleNode* node) { return node->PreviousSibling(); }
static void* pseudo_owner(StyleNode* node) {
  return node->PseudoElementOwner();
}

bool CSSJITCompiler::MatchSelector(const LynxCSSSelector* selector) {
  if (!MatchSimple(selector)) {
    return false;
  }
  if (selector->IsLastInTagHistory()) {
    return true;
  }

  auto relation = selector->Relation();
  auto next = selector->TagHistory();
  auto skip = __->Label("skip");

  __->GotoIfNot(skip, kReturnValueReg, kMatches, "if res != kMatches");

  bool success;
  if (relation == LynxCSSSelector::kSubSelector) {
    success = MatchSelector(next);
  } else {
    success = MatchForRelation(next, relation);
  }
  __->bind(skip);
  return success;
}

bool CSSJITCompiler::MatchForRelation(const LynxCSSSelector* selector,
                                      int relation) {
  switch (relation) {
    case LynxCSSSelector::kDescendant: {
      auto again = __->Label("again");
      auto fail = __->Label("fail");
      auto done = __->Label("done");
      __->Push(kElementReg);
      __->bind(again);

      __->Comment("GetParent");
      __->Call((void*)getparent, kElementReg);

      __->GotoIf(fail, kReturnValueReg, 0, "if nullptr");

      __->Mov(kElementReg, kReturnValueReg);

      if (!MatchSelector(selector)) {
        return false;
      }

      __->GotoIf(done, kReturnValueReg, kMatches, "if res = kMatches");
      __->GotoIf(done, kReturnValueReg, kFailsCompletely,
                 "if res = kFailsCompletely");
      __->Goto(again);

      __->bind(fail);
      __->Mov(kReturnValueReg, kFailsCompletely, "res = kFailsCompletely");

      __->bind(done);
      __->Pop(kElementReg);
      return true;
    }
    case LynxCSSSelector::kChild: {
      auto fail = __->Label("fail");
      auto done = __->Label("done");
      __->Comment("GetParent");
      __->Call((void*)getparent, kElementReg);

      __->GotoIf(fail, kReturnValueReg, 0, "if nullptr");

      __->Push(kElementReg);
      __->Mov(kElementReg, kReturnValueReg);

      if (!MatchSelector(selector)) {
        return false;
      }

      __->Pop(kElementReg);
      __->Goto(done);

      __->bind(fail);
      __->Mov(kReturnValueReg, kFailsCompletely, "res = kFailsCompletely");

      __->bind(done);
      return true;
    }
    case LynxCSSSelector::kDirectAdjacent: {
      auto fail = __->Label("fail");
      auto done = __->Label("done");
      __->Comment("GetPreviousSibling");
      __->Call((void*)&getpsib, kElementReg);

      __->GotoIf(fail, kReturnValueReg, 0, "if nullptr");

      __->Push(kElementReg);
      __->Mov(kElementReg, kReturnValueReg);

      if (!MatchSelector(selector)) {
        return false;
      }

      __->Pop(kElementReg);
      __->Goto(done);

      __->bind(fail);
      __->Mov(kReturnValueReg, kFailsAllSiblings, "res = kFailsAllSiblings");

      __->bind(done);
      return true;
    }
    case LynxCSSSelector::kIndirectAdjacent: {
      auto again = __->Label("again");
      auto fail = __->Label("fail");
      auto done = __->Label("done");
      __->Push(kElementReg);
      __->bind(again);

      __->Comment("GetPreviousSibling");
      __->Call((void*)&getpsib, kElementReg);

      __->GotoIf(fail, kReturnValueReg, 0, "if nullptr");

      __->Mov(kElementReg, kReturnValueReg);

      if (!MatchSelector(selector)) {
        return false;
      }

      __->GotoIf(done, kReturnValueReg, kMatches, "if res == kMatches");
      __->GotoIf(done, kReturnValueReg, kFailsAllSiblings,
                 "if res == kFailsAllSiblings");
      __->GotoIf(done, kReturnValueReg, kFailsCompletely,
                 "if res == kFailsCompletely");

      __->Goto(again);

      __->bind(fail);
      __->Mov(kReturnValueReg, "res = kFailsAllSiblings");

      __->bind(done);
      __->Pop(kElementReg);
      return true;
    }
    case LynxCSSSelector::kUAShadow: {
      __->Push(kElementReg);

      __->Comment("GetPseudoElementOwner");
      __->Call((void*)&pseudo_owner, kElementReg);

      __->Mov(kElementReg, kReturnValueReg);

      if (!MatchSelector(selector)) {
        return false;
      }

      __->Pop(kElementReg);
      return true;
    }
    default:
      break;
  }
  return kFailsCompletely;
}

bool CSSJITCompiler::MatchSimple(const LynxCSSSelector* selector) {
  switch (selector->Match()) {
    case LynxCSSSelector::kTag:
      if (selector->Value() == CSSGlobalStarString()) {
        __->Mov(kReturnValueReg, kMatches, "res = kMatches");
      } else {
        __->Comment("MatchTag");
        MatchString(selector->Value(), (void*)&matchtag);
      }
      return true;
    case LynxCSSSelector::kId:
      __->Comment("MatchId");
      MatchString(selector->Value(), (void*)&matchid);
      return true;
    case LynxCSSSelector::kClass:
      __->Comment("MatchClass");
      MatchString(selector->Value(), (void*)&matchcls);
      return true;
    case LynxCSSSelector::kPseudoClass:
      return MatchPseudoClass(selector);
    case LynxCSSSelector::kPseudoElement:
      return MatchPseudoElement(selector);
    default:
      return false;
  }
}

bool CSSJITCompiler::MatchPseudoNot(const LynxCSSSelector* selector) {
  DCHECK(selector->SelectorList());
  auto fail = __->Label("fail");
  auto done = __->Label("done");
  for (selector = selector->SelectorList()->First(); selector;
       selector = LynxCSSSelectorList::Next(*selector)) {
    if (!MatchSelector(selector)) {
      return false;
    }
    __->GotoIf(fail, kReturnValueReg, kMatches, "if res == kMatches");
  }

  __->Mov(kReturnValueReg, kMatches, "res = kMatches");
  __->Goto(done);

  __->bind(fail);
  __->Mov(kReturnValueReg, kFailsLocally, "res = kFailsLocally");

  __->bind(done);
  return true;
}

bool CSSJITCompiler::MatchPseudoClass(const LynxCSSSelector* selector) {
  switch (selector->GetPseudoType()) {
    case LynxCSSSelector::kPseudoNot:
      __->Comment("/* MatchPseudoNot */", false);
      return MatchPseudoNot(selector);
    case LynxCSSSelector::kPseudoHover:
      MatchPseudoState(selector, tasm::kPseudoStateHover);
      return true;
    case LynxCSSSelector::kPseudoActive:
      MatchPseudoState(selector, tasm::kPseudoStateActive);
      return true;
    case LynxCSSSelector::kPseudoFocus:
      MatchPseudoState(selector, tasm::kPseudoStateFocus);
      return true;
    case LynxCSSSelector::kPseudoRoot:
      __->Comment("MatchPseudoRoot");
      __->Call((void*)&isroot, kElementReg);
      return true;
    default:
      break;
  }
  return false;
}

bool CSSJITCompiler::MatchPseudoElement(const LynxCSSSelector* selector) {
  switch (selector->GetPseudoType()) {
    case LynxCSSSelector::PseudoType::kPseudoPlaceholder:
      MatchPseudoState(selector, tasm::kPseudoStatePlaceHolder);
      return true;
    case LynxCSSSelector::PseudoType::kPseudoSelection:
      MatchPseudoState(selector, tasm::kPseudoStateSelection);
      return true;
    default:
      break;
  }
  return false;
}

void CSSJITCompiler::MatchPseudoState(const LynxCSSSelector* selector,
                                      uint32_t state) {
  __->Comment("MatchPseudo");
  __->Call((void*)&matchpseudo, kElementReg, state, selector->Value().c_str());
}

void CSSJITCompiler::MatchString(const std::string& str, void* fn) {
  __->Call(fn, kElementReg, str.c_str(), str.size(), str.c_str(), "length");
}

JitCodeMatcher CSSJITCompiler::Compile(const LynxCSSSelector* selector) {
  JitCodeMatcher fn = nullptr;
  __ = new MacroAssembler(rt_);
  __->Prolog();
  bool success = MatchSelector(selector);
  if (success) {
    __->Epilog();
    fn = __->Generate();
  }
  delete __;
  return fn;
}

CSSJITCompiler::CSSJITCompiler() { rt_ = new asmjit::JitRuntime; }

CSSJITCompiler::~CSSJITCompiler() { delete rt_; }

}  // namespace css
}  // namespace lynx
