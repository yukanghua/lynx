// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <utility>

#include "base/include/fml/synchronization/waitable_event.h"
#include "base/include/fml/thread.h"
#include "base/include/string/string_utils.h"
#include "core/renderer/css/ng/css_ng_utils.h"
#include "core/renderer/css/ng/cssjit_compiler.h"
#include "core/renderer/css/ng/matcher/selector_matcher.h"
#include "core/renderer/css/ng/selector/lynx_css_selector_list.h"

// cspell:disable
namespace lynx {
namespace css {

static fml::Thread* jit_work_thread_ = nullptr;
static CSSJITCompiler* jit_instance_ = nullptr;

// static
void CSSJITCompiler::CompileOnBackgroundThread(LynxCSSSelector* selector) {
  static std::once_flag init_flag;
  std::call_once(init_flag, [] {
    jit_work_thread_ = new fml::Thread("CSSJIT_BGThread");
    jit_work_thread_->GetTaskRunner()->PostTask(
        []() { jit_instance_ = new CSSJITCompiler; });
  });

  jit_work_thread_->GetTaskRunner()->PostTask([selector]() {
    if (jit_instance_) {
      auto fn = jit_instance_->Compile(selector);
      if (fn) {
        selector->SetJITCode((void*)fn);
      }
    }
  });
}

// static
void CSSJITCompiler::WaitBackgroundThreadIdle() {
  if (jit_work_thread_) {
    fml::AutoResetWaitableEvent arwe;
    jit_work_thread_->GetTaskRunner()->PostTask([&arwe]() { arwe.Signal(); });
    arwe.Wait();
  }
}

}  // namespace css
}  // namespace lynx
