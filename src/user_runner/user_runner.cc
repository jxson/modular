// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the user runner mojo app.

#include <memory>

#include "apps/ledger/services/ledger.fidl.h"
#include "apps/modular/lib/app/application_context.h"
#include "apps/modular/lib/app/connect.h"
#include "apps/modular/lib/fidl/array_to_string.h"
#include "apps/modular/lib/fidl/strong_binding.h"
#include "apps/modular/services/user/user_runner.fidl.h"
#include "apps/modular/services/user/user_shell.fidl.h"
#include "apps/modular/src/user_runner/story_provider_impl.h"
#include "apps/mozart/services/views/view_provider.fidl.h"
#include "apps/mozart/services/views/view_token.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/fidl/cpp/bindings/interface_ptr_set.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "lib/mtl/tasks/message_loop.h"

namespace modular {

namespace {

const char kAppId[] = "modular_user_runner";

std::string LedgerStatusToString(ledger::Status status) {
  switch (status) {
    case ledger::Status::OK:
      return "OK";
    case ledger::Status::AUTHENTICATION_ERROR:
      return "AUTHENTICATION_ERROR";
    case ledger::Status::PAGE_NOT_FOUND:
      return "PAGE_NOT_FOUND";
    case ledger::Status::KEY_NOT_FOUND:
      return "KEY_NOT_FOUND";
    case ledger::Status::REFERENCE_NOT_FOUND:
      return "REFERENCE_NOT_FOUND";
    case ledger::Status::IO_ERROR:
      return "IO_ERROR";
    case ledger::Status::TRANSACTION_ALREADY_IN_PROGRESS:
      return "TRANSACTION_ALREADY_IN_PROGRESS";
    case ledger::Status::NO_TRANSACTION_IN_PROGRESS:
      return "NO_TRANSACTION_IN_PROGRESS";
    case ledger::Status::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case ledger::Status::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    default:
      return "(unknown error)";
  }
};

}  // namespace

// Creates an ApplicationEnvironment at the UserRunner scope. This environment
// provides services like Ledger as an environment service to applications
// running in its scope like User Shell, Story Runner.
class UserRunnerScope : public ApplicationEnvironmentHost {
 public:
  UserRunnerScope(std::shared_ptr<ApplicationContext> application_context,
                  fidl::Array<uint8_t> user_id)
      : application_context_(application_context),
        binding_(this),
        user_id_(std::move(user_id)) {
    // Set up ApplicationEnvironment.
    ApplicationEnvironmentHostPtr env_host;
    binding_.Bind(fidl::GetProxy(&env_host));
    application_context_->environment()->CreateNestedEnvironment(
        std::move(env_host), fidl::GetProxy(&env_), GetProxy(&env_controller_));

    // Register and set up Services hosted in this environment.
    RegisterServices();
    SetupLedger();
  }

  ApplicationEnvironmentPtr GetEnvironment() {
    ApplicationEnvironmentPtr env;
    env_->Duplicate(fidl::GetProxy(&env));
    return env;
  }

 private:
  // |ApplicationEnvironmentHost|:
  void GetApplicationEnvironmentServices(
      fidl::InterfaceRequest<ServiceProvider> environment_services) override {
    env_services_.AddBinding(std::move(environment_services));
  }

  void RegisterServices() {
    env_services_.AddService<ledger::Ledger>(
        [this](fidl::InterfaceRequest<ledger::Ledger> request) {
          FTL_DLOG(INFO) << "Servicing Ledger service request";
          // TODO(alhaad): Once supported by Ledger, only create a user scoped
          // ledger here.
          fidl::StructPtr<ledger::Identity> identity = ledger::Identity::New();
          identity->user_id = user_id_.Clone();
          identity->app_id = to_array(kAppId);
          ledger_factory_->GetLedger(
              std::move(identity), std::move(request),
              [this](ledger::Status status) {
                if (status != ledger::Status::OK) {
                  FTL_LOG(ERROR) << "UserRunnerScope::"
                                    "GetApplicationEnvironmentServices():"
                                 << " LedgerFactory.GetLedger() failed:"
                                 << " " << LedgerStatusToString(status) << ".";
                }
              });
        });

    env_services_.SetDefaultServiceConnector(
        [this](std::string service_name, mx::channel channel) {
          FTL_DLOG(INFO) << "Servicing default service request for "
                         << service_name;
          application_context_->environment_services()->ConnectToService(
              service_name, std::move(channel));
        });
  }

  void SetupLedger() {
    auto launch_info = ApplicationLaunchInfo::New();

    ServiceProviderPtr app_services;
    launch_info->services = GetProxy(&app_services);
    launch_info->url = "file:///system/apps/ledger";

    // Note that |LedgerFactory| is started in the device runner's environment.
    application_context_->launcher()->CreateApplication(std::move(launch_info),
                                                        nullptr);

    ConnectToService(app_services.get(), GetProxy(&ledger_factory_));
  }

  std::shared_ptr<ApplicationContext> application_context_;
  fidl::Binding<ApplicationEnvironmentHost> binding_;

  ApplicationEnvironmentPtr env_;
  ApplicationEnvironmentControllerPtr env_controller_;
  ServiceProviderImpl env_services_;

  fidl::Array<uint8_t> user_id_;

  // Services hosted in this environment.
  ledger::LedgerFactoryPtr ledger_factory_;
};

class UserRunnerImpl : public UserRunner {
 public:
  UserRunnerImpl(std::shared_ptr<ApplicationContext> application_context,
                 fidl::InterfaceRequest<UserRunner> user_runner_request)
      : application_context_(application_context),
        binding_(this, std::move(user_runner_request)) {}

  ~UserRunnerImpl() override = default;

 private:
  // |UserRunner|:
  void Launch(
      fidl::Array<uint8_t> user_id,
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request) override {
    FTL_LOG(INFO) << "UserRunnerImpl::Launch()";

    user_runner_scope_ = std::make_unique<UserRunnerScope>(application_context_,
                                                           std::move(user_id));

    RunUserShell(std::move(view_owner_request));

    ServiceProviderPtr environment_services;
    user_runner_scope_->GetEnvironment()->GetServices(
        GetProxy(&environment_services));
    fidl::InterfaceHandle<StoryProvider> story_provider;
    new StoryProviderImpl(
        user_runner_scope_->GetEnvironment(),
        ConnectToService<ledger::Ledger>(environment_services.get()),
        GetProxy(&story_provider));

    user_shell_->SetStoryProvider(std::move(story_provider));
  }

  // This method starts UserShell in a new process, connects to its
  // |ViewProvider| interface, passes a |ViewOwner| request, gets
  // |ServiceProvider| and finally connects to UserShell.
  void RunUserShell(
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request) {
    auto launch_info = ApplicationLaunchInfo::New();

    ServiceProviderPtr app_services;
    launch_info->services = GetProxy(&app_services);
    launch_info->url = "file:///system/apps/dummy_user_shell";

    ApplicationLauncherPtr launcher;
    user_runner_scope_->GetEnvironment()->GetApplicationLauncher(
        fidl::GetProxy(&launcher));
    launcher->CreateApplication(std::move(launch_info), nullptr);

    mozart::ViewProviderPtr view_provider;
    ConnectToService(app_services.get(), GetProxy(&view_provider));

    ServiceProviderPtr view_services;
    view_provider->CreateView(std::move(view_owner_request),
                              GetProxy(&view_services));

    view_providers_.AddInterfacePtr(std::move(view_provider));

    // Use this service provider to get |UserShell| interface.
    ConnectToService(view_services.get(), GetProxy(&user_shell_));
  }

  std::shared_ptr<ApplicationContext> application_context_;
  StrongBinding<UserRunner> binding_;

  // The application environment hosted by user runner.
  std::unique_ptr<UserRunnerScope> user_runner_scope_;

  fidl::InterfacePtrSet<mozart::ViewProvider> view_providers_;
  UserShellPtr user_shell_;

  FTL_DISALLOW_COPY_AND_ASSIGN(UserRunnerImpl);
};

class UserRunnerApp {
 public:
  UserRunnerApp()
      : application_context_(ApplicationContext::CreateFromStartupInfo()) {
    application_context_->outgoing_services()->AddService<UserRunner>(
        [this](fidl::InterfaceRequest<UserRunner> request) {
          new UserRunnerImpl(application_context_, std::move(request));
        });
  }

 private:
  std::shared_ptr<ApplicationContext> application_context_;
  FTL_DISALLOW_COPY_AND_ASSIGN(UserRunnerApp);
};

}  // namespace modular

int main(int argc, const char** argv) {
  mtl::MessageLoop loop;
  modular::UserRunnerApp app;
  loop.Run();
  return 0;
}