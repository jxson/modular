// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a dummy User Shell. This passes a dummy user name
// to Device Runner.

#include <mojo/system/main.h>

#include "apps/modular/device_runner/device_runner.mojom.h"
#include "apps/modular/mojo/single_service_application.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "mojo/public/cpp/application/application_impl_base.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/run_application.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace modular {

constexpr char kDummyUserName[] = "user1";

using mojo::ApplicationImplBase;
using mojo::ConnectionContext;
using mojo::InterfaceHandle;
using mojo::InterfacePtr;
using mojo::InterfaceRequest;
using mojo::ServiceProviderImpl;
using mojo::StrongBinding;

class DummyDeviceShellImpl : public DeviceShell {
 public:
  explicit DummyDeviceShellImpl(InterfaceRequest<DeviceShell> request)
      : binding_(this, std::move(request)) {}
  ~DummyDeviceShellImpl() override{};

  void SetDeviceRunner(InterfaceHandle<DeviceRunner> device_runner) override {
    device_runner_ = InterfacePtr<DeviceRunner>::Create(device_runner.Pass());
    device_runner_->Login(kDummyUserName);
  }

 private:
  StrongBinding<DeviceShell> binding_;
  InterfacePtr<DeviceRunner> device_runner_;
  FTL_DISALLOW_COPY_AND_ASSIGN(DummyDeviceShellImpl);
};

}  // namespace

MojoResult MojoMain(MojoHandle application_request) {
  FTL_LOG(INFO) << "dummy_device_shell main";
  modular::SingleServiceApplication<modular::DeviceShell, modular::DummyDeviceShellImpl> app;
  return mojo::RunApplication(application_request, &app);
}