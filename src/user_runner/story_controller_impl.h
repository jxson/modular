// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_USER_RUNNER_STORY_CONTROLLER_IMPL_H_
#define APPS_MODULAR_SRC_USER_RUNNER_STORY_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "apps/modular/lib/document_editor/document_editor.h"
#include "apps/modular/services/application/application_launcher.fidl.h"
#include "apps/modular/services/story/story_runner.fidl.h"
#include "apps/modular/services/user/story_data.fidl.h"
#include "apps/modular/services/user/story_provider.fidl.h"
#include "apps/modular/services/user/user_runner.fidl.h"
#include "apps/modular/src/user_runner/story_storage_impl.h"
#include "apps/modular/src/user_runner/user_ledger_repository_factory.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_ptr_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/ftl/macros.h"

namespace modular {
class ApplicationContext;
class StoryProviderImpl;

class StoryControllerImpl : public StoryController,
                            public ModuleWatcher,
                            public LinkWatcher {
 public:
  static StoryControllerImpl* New(
      StoryDataPtr story_data,
      StoryProviderImpl* story_provider_impl,
      ApplicationLauncherPtr launcher,
      UserLedgerRepositoryFactory* const ledger_repository_factory) {
    return new StoryControllerImpl(std::move(story_data), story_provider_impl,
                                   std::move(launcher),
                                   ledger_repository_factory);
  }

  ~StoryControllerImpl() override = default;

  void Connect(fidl::InterfaceRequest<StoryController> story_controller_request);

  size_t bindings_size() const { return bindings_.size(); }

 private:
  // Constructor is private to ensure (by way of New()) that instances
  // are created always with new.
  StoryControllerImpl(
      StoryDataPtr story_data,
      StoryProviderImpl* story_provider_impl,
      ApplicationLauncherPtr launcher,
      UserLedgerRepositoryFactory* ledger_repository_factory);

  // |StoryController|
  void GetInfo(const GetInfoCallback& callback) override;
  void SetInfoExtra(const fidl::String& name,
                    const fidl::String& value,
                    const SetInfoExtraCallback& callback) override;
  void Start(
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request) override;
  void GetLink(fidl::InterfaceRequest<Link> link_request) override;
  void Stop(const StopCallback& done) override;
  void Watch(fidl::InterfaceHandle<StoryWatcher> story_watcher) override;

  // |ModuleWatcher|
  void OnDone() override;
  void OnStop() override;
  void OnError() override;

  // |LinkWatcher|
  void Notify(FidlDocMap docs) override;

  void WriteStoryData(std::function<void()> done);
  void NotifyStoryWatchers(void (StoryWatcher::*method)());

  // Starts the Story instance for the given story.
  void StartStory(fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request);

  // Tears down the currently used StoryRunner instance, if any.
  void TearDownStory(std::function<void()> done);

  StoryDataPtr story_data_;
  StoryProviderImpl* const story_provider_impl_;
  std::shared_ptr<StoryStorageImpl::Storage> storage_;
  std::unique_ptr<StoryStorageImpl> story_storage_impl_;
  ApplicationLauncherPtr launcher_;

  fidl::BindingSet<StoryController> bindings_;
  fidl::Binding<ModuleWatcher> module_watcher_binding_;
  fidl::Binding<LinkWatcher> link_changed_binding_;
  fidl::InterfacePtrSet<StoryWatcher> story_watchers_;
  StoryPtr story_;
  StoryContextPtr story_context_;
  LinkPtr root_;
  // If requests for root_ arrive before we have it, we store these
  // requests here.
  std::vector<fidl::InterfaceRequest<Link>> root_requests_;
  ModuleControllerPtr module_;

  UserLedgerRepositoryFactory* const ledger_repository_factory_;

  FTL_DISALLOW_COPY_AND_ASSIGN(StoryControllerImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_USER_RUNNER_STORY_IMPL_H_
