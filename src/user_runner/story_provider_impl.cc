// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/user_runner/story_provider_impl.h"

#include <stdlib.h>
#include <time.h>
#include <unordered_set>

#include "apps/modular/lib/app/connect.h"
#include "apps/modular/lib/fidl/array_to_string.h"
#include "apps/modular/src/user_runner/story_controller_impl.h"
#include "lib/fidl/cpp/bindings/array.h"
#include "lib/fidl/cpp/bindings/interface_handle.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/ftl/functional/make_copyable.h"

namespace modular {
namespace {

void InitStoryId() {
  // If rand() is not seeded, it always returns the same sequence of numbers.
  srand(time(nullptr));
}

// Generates a unique randomly generated string of |length| size to be
// used as a story id.
std::string MakeStoryId(std::unordered_set<std::string>* story_ids,
                        const size_t length) {
  std::function<char()> randchar = []() -> char {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };

  std::string id(length, 0);
  std::generate_n(id.begin(), length, randchar);

  if (story_ids->find(id) != story_ids->end()) {
    return MakeStoryId(story_ids, length);
  }

  story_ids->insert(id);
  return id;
}

// Below are helper classes that encapsulates a chain of asynchronous
// operations on the Ledger. Because the operations all return
// something, the handles on which they are invoked need to be kept
// around until the return value arrives. This precludes them to be
// local variables. The next thing that comes to mind is to make them
// fields of the containing object. However, there might be multiple
// such operations going on concurrently in one StoryProviderImpl, so
// they cannot be fields of StoryProviderImpl. Thus such operations
// are separate classes for now, until I can think of something
// better, or we change the API to interface requests.
//
// NOTE(mesch): After these classes were written, the API was changed
// to InterfaceRequests. Most of the nesting can be removed now,
// unless we want to check status, which is still returned. Status
// checking was useful in debugging ledger, so I let the nesting in
// place for now.

class GetStoryDataCall : public Transaction {
 public:
  using Result = std::function<void(StoryDataPtr)>;

  GetStoryDataCall(TransactionContainer* const container,
                   ledger::Ledger* const ledger,
                   const fidl::String& story_id,
                   Result result)
      : Transaction(container),
        ledger_(ledger),
        story_id_(story_id),
        result_(result) {
    ledger_->GetRootPage(GetProxy(&root_page_), [this](ledger::Status status) {
      if (status != ledger::Status::OK) {
        FTL_LOG(ERROR) << "GetStoryDataCall() " << story_id_
                       << " Ledger.GetRootPage() " << status;
        Done();
        return;
      }

      root_page_->GetSnapshot(
          GetProxy(&root_snapshot_), [this](ledger::Status status) {
            if (status != ledger::Status::OK) {
              FTL_LOG(ERROR) << "GetStoryDataCall() " << story_id_
                             << " Page.GetSnapshot() " << status;
              Done();
              return;
            }

            root_snapshot_->Get(
                to_array(story_id_),
                [this](ledger::Status status, ledger::ValuePtr value) {
                  if (status != ledger::Status::OK) {
                    FTL_LOG(ERROR) << "GetStoryDataCall() " << story_id_
                                   << " PageSnapshot.Get() " << status;
                    // TODO(mesch): This does not succeed if the story
                    // was deleted. This is not an error but a
                    // legitimate condition. Close the loop here.
                    Done();
                    return;
                  }

                  story_data_ = StoryData::New();
                  story_data_->Deserialize(value->get_bytes().data(),
                                           value->get_bytes().size());

                  result_(std::move(story_data_));
                  Done();
                });
          });
    });
  };

 private:
  ledger::Ledger* const ledger_;  // not owned
  const fidl::String story_id_;
  Result result_;

  ledger::PagePtr root_page_;
  ledger::PageSnapshotPtr root_snapshot_;
  StoryDataPtr story_data_;

  FTL_DISALLOW_COPY_AND_ASSIGN(GetStoryDataCall);
};

class WriteStoryDataCall : public Transaction {
 public:
  using Result = std::function<void()>;

  WriteStoryDataCall(TransactionContainer* const container,
                     ledger::Ledger* const ledger,
                     StoryDataPtr story_data,
                     Result result)
      : Transaction(container),
        ledger_(ledger),
        story_data_(std::move(story_data)),
        result_(result) {
    FTL_DCHECK(!story_data_.is_null());

    ledger_->GetRootPage(GetProxy(&root_page_), [this](ledger::Status status) {
      const size_t size = story_data_->GetSerializedSize();
      fidl::Array<uint8_t> value = fidl::Array<uint8_t>::New(size);
      story_data_->Serialize(value.data(), size);

      const fidl::String& story_id = story_data_->story_info->id;
      root_page_->PutWithPriority(to_array(story_id), std::move(value),
                                  ledger::Priority::EAGER,
                                  [this](ledger::Status status) {
                                    result_();
                                    Done();
                                  });
    });
  }

 private:
  ledger::Ledger* const ledger_;  // not owned
  StoryDataPtr story_data_;
  ledger::PagePtr root_page_;
  Result result_;

  FTL_DISALLOW_COPY_AND_ASSIGN(WriteStoryDataCall);
};

class CreateStoryCall : public Transaction {
 public:
  CreateStoryCall(
      TransactionContainer* const container,
      ledger::Ledger* const ledger,
      ApplicationEnvironment* const environment,
      StoryProviderImpl* const story_provider_impl,
      const fidl::String& url,
      const std::string& story_id,
      UserLedgerRepositoryFactory* ledger_repository_factory)
      : Transaction(container),
        ledger_(ledger),
        environment_(environment),
        story_provider_impl_(story_provider_impl),
        url_(url),
        story_id_(story_id),
        ledger_repository_factory_(ledger_repository_factory) {
    ledger_->NewPage(GetProxy(&story_page_), [this](ledger::Status status) {
      story_page_->GetId([this](fidl::Array<uint8_t> story_page_id) {
        story_data_ = StoryData::New();
        story_data_->story_page_id = std::move(story_page_id);
        story_data_->story_info = StoryInfo::New();
        auto* const story_info = story_data_->story_info.get();
        story_info->url = url_;
        story_info->id = story_id_;
        story_info->is_running = false;
        story_info->state = StoryState::NEW;
        story_info->extra.mark_non_null();

        story_provider_impl_->WriteStoryData(story_data_->Clone(), [this]() {
          ApplicationLauncherPtr launcher;
          environment_->GetApplicationLauncher(fidl::GetProxy(&launcher));
          story_provider_impl_->AddController(story_id_, StoryControllerImpl::New(
              std::move(story_data_), story_provider_impl_, std::move(launcher),
              ledger_repository_factory_));
          Done();
        });
      });
    });
  }

 private:
  ledger::Ledger* const ledger_;  // not owned
  ApplicationEnvironment* const environment_;
  StoryProviderImpl* const story_provider_impl_;  // not owned
  const fidl::String url_;
  const std::string story_id_;
  UserLedgerRepositoryFactory* const ledger_repository_factory_;  // not owned

  ledger::PagePtr story_page_;
  StoryDataPtr story_data_;

  FTL_DISALLOW_COPY_AND_ASSIGN(CreateStoryCall);
};

// Deletes a story given its id.
class DeleteStoryCall : public Transaction {
 public:
  using Result = StoryProviderImpl::DeleteStoryCallback;

  DeleteStoryCall(TransactionContainer* const container,
                  ledger::Ledger* const ledger,
                  const fidl::String& story_id,
                  Result result)
      : Transaction(container),
        ledger_(ledger),
        story_id_(story_id),
        result_(result) {
    ledger_->GetRootPage(GetProxy(&root_page_), [this](ledger::Status status) {
      root_page_->Delete(to_array(story_id_),
                         [this](ledger::Status ledger_status) {
                           result_();
                           Done();
                         });
    });
  }

 private:
  ledger::Ledger* const ledger_;  // not owned
  ledger::PagePtr root_page_;
  const fidl::String story_id_;
  Result result_;

  FTL_DISALLOW_COPY_AND_ASSIGN(DeleteStoryCall);
};

class ResumeStoryCall : public Transaction {
 public:
  ResumeStoryCall(
      TransactionContainer* const container,
      ledger::Ledger* const ledger,
      ApplicationEnvironment* const environment,
      StoryProviderImpl* const story_provider_impl,
      const fidl::String& story_id,
      UserLedgerRepositoryFactory* ledger_repository_factory)
      : Transaction(container),
        ledger_(ledger),
        environment_(environment),
        story_provider_impl_(story_provider_impl),
        story_id_(story_id),
        ledger_repository_factory_(ledger_repository_factory) {
    story_provider_impl_->GetStoryData(
        story_id_, [this](StoryDataPtr story_data) {
          // TODO(mesch): This does not succeed if the story was
          // deleted. Close the loop here.
          story_data_ = std::move(story_data);
          ledger_->GetPage(
              story_data_->story_page_id.Clone(), GetProxy(&story_page_),
              [this](ledger::Status status) {
                ApplicationLauncherPtr launcher;
                environment_->GetApplicationLauncher(fidl::GetProxy(&launcher));
                story_provider_impl_->AddController(story_id_, StoryControllerImpl::New(
                    std::move(story_data_), story_provider_impl_,
                    std::move(launcher), ledger_repository_factory_));
                Done();
              });
        });
  }

 private:
  ledger::Ledger* const ledger_;  // not owned
  ApplicationEnvironment* const environment_;
  StoryProviderImpl* const story_provider_impl_;  // not owned
  const fidl::String story_id_;
  UserLedgerRepositoryFactory* const ledger_repository_factory_;  // not owned

  StoryDataPtr story_data_;
  ledger::PagePtr story_page_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ResumeStoryCall);
};

class PreviousStoriesCall : public Transaction {
 public:
  using Result = StoryProviderImpl::PreviousStoriesCallback;

  PreviousStoriesCall(TransactionContainer* const container,
                      ledger::Ledger* const ledger,
                      Result result)
      : Transaction(container), ledger_(ledger), result_(result) {
    ledger_->GetRootPage(GetProxy(&root_page_), [this](ledger::Status status) {
      root_page_->GetSnapshot(
          GetProxy(&root_snapshot_), [this](ledger::Status status) {
            root_snapshot_->GetEntries(
                nullptr, nullptr, [this](ledger::Status status,
                                         fidl::Array<ledger::EntryPtr> entries,
                                         fidl::Array<uint8_t> next_token) {
                  // TODO(mesch): Account for possibly
                  // continuation here. That's not just a matter
                  // of repeatedly calling, but it needs to be
                  // wired up to the API, because a list that is
                  // too large to return from Ledger is also too
                  // large to return from StoryProvider.
                  fidl::Array<fidl::String> story_ids;
                  // This resize() has the side effect of marking the array as
                  // non-null. Do not remove it because the fidl declaration
                  // of this return value does not allow nulls.
                  story_ids.resize(0);
                  for (auto& entry : entries) {
                    StoryDataPtr story_data = StoryData::New();
                    story_data->Deserialize(entry->value.data(),
                                            entry->value.size());
                    story_ids.push_back(story_data->story_info->id);
                  }

                  result_(std::move(story_ids));
                  Done();
                });
          });
    });
  }

 private:
  ledger::Ledger* const ledger_;  // not owned
  Result result_;
  ledger::PagePtr root_page_;
  ledger::PageSnapshotPtr root_snapshot_;

  FTL_DISALLOW_COPY_AND_ASSIGN(PreviousStoriesCall);
};

}  // namespace

StoryProviderImpl::StoryProviderImpl(
    ApplicationEnvironmentPtr environment,
    fidl::InterfaceHandle<ledger::Ledger> ledger,
    fidl::InterfaceRequest<StoryProvider> story_provider_request,
    UserLedgerRepositoryFactory* const ledger_repository_factory)
    : environment_(std::move(environment)),
      binding_(this),
      storage_(new Storage),
      page_watcher_binding_(this),
      ledger_repository_factory_(ledger_repository_factory) {
  FTL_DCHECK(ledger_repository_factory_);

  ledger_.Bind(std::move(ledger));

  ledger::PagePtr root_page;
  ledger_->GetRootPage(GetProxy(&root_page), [this](ledger::Status status) {
    if (status != ledger::Status::OK) {
      FTL_LOG(ERROR)
          << "StoryProviderImpl() failed call to Ledger.GetRootPage() "
          << status;
    }
  });

  fidl::InterfaceHandle<ledger::PageWatcher> watcher;
  page_watcher_binding_.Bind(GetProxy(&watcher));
  root_page->Watch(std::move(watcher), [](ledger::Status status) {
    if (status != ledger::Status::OK) {
      FTL_LOG(ERROR) << "StoryProviderImpl() failed call to Ledger.Watch() "
                     << status;
    }
  });

  // We must initialize story_ids_ with the IDs of currently existing
  // stories *before* we can process any calls that might create a new
  // story. Hence we bind the interface request only after this call
  // completes.
  new PreviousStoriesCall(
      &transaction_container_, ledger_.get(), ftl::MakeCopyable([
        this, story_provider_request = std::move(story_provider_request)
      ](fidl::Array<fidl::String> stories) mutable {
        for (auto& story_id : stories) {
          story_ids_.insert(story_id.get());
        }

        InitStoryId();  // So MakeStoryId() returns something more random.

        binding_.Bind(std::move(story_provider_request));
      }));
}

// Announces the eventual arrival of a controller instance. If another
// request arrives for it meanwhile, it is stored in the entry here
// until the first request finishes. All requests received by then are
// then connected to the controller instance in AddController() below.
void StoryProviderImpl::ReserveController(
    const std::string& story_id,
    fidl::InterfaceRequest<StoryController> story_controller_request) {
  if (story_controllers_.find(story_id) == story_controllers_.end()) {
    story_controllers_[story_id].reset(new StoryControllerEntry);
  }
  story_controllers_[story_id]->requests.emplace_back(
      std::move(story_controller_request));
}

// Completes the asynchronous creation of a controller instance
// started by ReserveController() above.
void StoryProviderImpl::AddController(
    const std::string& story_id, StoryControllerImpl* const story_controller) {
  FTL_DCHECK(story_controllers_.find(story_id) != story_controllers_.end());
  FTL_DCHECK(story_controllers_[story_id].get() != nullptr);
  FTL_DCHECK(story_controllers_[story_id]->impl.get() == nullptr);

  StoryControllerEntry* const entry = story_controllers_[story_id].get();
  entry->impl.reset(story_controller);
  for (auto& request : entry->requests) {
    story_controller->Connect(std::move(request));
  }
  entry->requests.clear();

  PurgeControllers();
}

void StoryProviderImpl::PurgeControllers() {
  // Scan entries for controllers without connections, or for those
  // marked deleted that have no requests pending. This is simpler
  // than to handle the on empty callback from the binding set.
  std::vector<std::string> disconnected;
  for (auto& entry : story_controllers_) {
    if ((entry.second->impl.get() != nullptr &&
         entry.second->impl->bindings_size() == 0) ||
        (entry.second->deleted && entry.second->requests.size() == 0)) {
      disconnected.push_back(entry.first);
    }
  }
  for (auto& story_id : disconnected) {
    FTL_LOG(INFO) << "StoryProviderImpl purge StoryController " << story_id;
    story_controllers_.erase(story_id);
  }
}

// |StoryProvider|
void StoryProviderImpl::Watch(
    fidl::InterfaceHandle<StoryProviderWatcher> watcher) {
  watchers_.AddInterfacePtr(
      StoryProviderWatcherPtr::Create(std::move(watcher)));
}

void StoryProviderImpl::GetStoryInfo(
    const fidl::String& story_id,
    const GetStoryInfoCallback& story_data_callback) {
  new GetStoryDataCall(&transaction_container_, ledger_.get(), story_id,
                       [this, story_data_callback](StoryDataPtr story_data) {
                         story_data_callback(std::move(story_data->story_info));
                       });
}

void StoryProviderImpl::GetStoryData(
    const fidl::String& story_id,
    const std::function<void(StoryDataPtr)>& result) {
  new GetStoryDataCall(&transaction_container_, ledger_.get(), story_id,
                       result);
}

ledger::PagePtr StoryProviderImpl::GetStoryPage(
    const fidl::Array<uint8_t>& story_page_id) {
  ledger::PagePtr ret;
  ledger_->GetPage(story_page_id.Clone(), GetProxy(&ret),
                   [](ledger::Status status) {
                     if (status != ledger::Status::OK) {
                       FTL_LOG(ERROR) << "GetStoryPage() status " << status;
                     }
                   });

  return ret;
}

void StoryProviderImpl::WriteStoryData(StoryDataPtr story_data,
                                       std::function<void()> done) {
  new WriteStoryDataCall(&transaction_container_, ledger_.get(),
                         std::move(story_data), done);
}

// |StoryProvider|
void StoryProviderImpl::CreateStory(
    const fidl::String& url,
    fidl::InterfaceRequest<StoryController> story_controller_request) {
  const std::string story_id = MakeStoryId(&story_ids_, 10);
  ReserveController(story_id, std::move(story_controller_request));
  new CreateStoryCall(
      &transaction_container_, ledger_.get(), environment_.get(), this, url,
      story_id, ledger_repository_factory_);
}

// |StoryProvider|
void StoryProviderImpl::DeleteStory(const fidl::String& story_id,
                                    const DeleteStoryCallback& callback) {
  // TODO(mesch): There is a big gaping race condition between
  // DeleteStory(), GetStoryInfo(), and ResumeStory(). Andrew already
  // found it.
  new DeleteStoryCall(&transaction_container_, ledger_.get(), story_id,
                      callback);
}

// |StoryProvider|
void StoryProviderImpl::ResumeStory(
    const fidl::String& story_id,
    fidl::InterfaceRequest<StoryController> story_controller_request) {
  if (story_controllers_.find(story_id) == story_controllers_.end()) {
    ReserveController(story_id, std::move(story_controller_request));
    new ResumeStoryCall(
        &transaction_container_, ledger_.get(), environment_.get(), this,
        story_id, ledger_repository_factory_);

  } else if (story_controllers_[story_id]->impl.get() != nullptr) {
    story_controllers_[story_id]->impl->Connect(std::move(
        story_controller_request));

  } else {
    ReserveController(story_id, std::move(story_controller_request));
  }
}

// |StoryProvider|
void StoryProviderImpl::PreviousStories(
    const PreviousStoriesCallback& callback) {
  new PreviousStoriesCall(&transaction_container_, ledger_.get(), callback);
}

// |PageWatcher|
void StoryProviderImpl::OnInitialState(
    fidl::InterfaceHandle<ledger::PageSnapshot> page,
    const OnInitialStateCallback& cb) {
  // TODO(mesch): Consider to initialize story_ids_ here, but the
  // current place may be better anyway.
  cb();
}

// |PageWatcher|
void StoryProviderImpl::OnChange(ledger::PageChangePtr page,
                                 const OnChangeCallback& cb) {
  FTL_DCHECK(!page.is_null());
  FTL_DCHECK(!page->changes.is_null());

  for (auto& entry : page->changes) {
    if (entry->new_value.is_null()) {
      const fidl::String story_id = to_string(entry->key);
      watchers_.ForAllPtrs([&story_id](StoryProviderWatcher* const watcher) {
        watcher->OnDelete(story_id);
      });

      // If there is a story controller entry for this ID, mark it
      // deleted and purge. We can only delete it when there are no
      // pending requests for it, otherwise we have to wait for the
      // requests to come back.
      auto i = story_controllers_.find(story_id);
      if (i != story_controllers_.end()) {
        FTL_DCHECK(i->second.get() != nullptr);
        i->second->deleted = true;
        PurgeControllers();
      }

    } else {
      auto story_data = StoryData::New();
      auto& bytes = entry->new_value->get_bytes();
      story_data->Deserialize(bytes.data(), bytes.size());

      // If this is a new story, guard against double using its key.
      story_ids_.insert(story_data->story_info->id.get());

      watchers_.ForAllPtrs([&story_data](StoryProviderWatcher* const watcher) {
        watcher->OnChange(story_data->story_info.Clone());
      });
    }
  }

  cb();
}

}  // namespace modular
