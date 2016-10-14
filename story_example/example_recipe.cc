// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Module that serves as the recipe in the example story, i.e. that
// creates other Modules in the session.

#include <mojo/system/main.h>
#include <vector>

#include "apps/maxwell/document_store/interfaces/document.mojom.h"
#include "apps/modular/document_editor/document_editor.h"
#include "apps/modular/mojo/single_service_view_app.h"
#include "apps/modular/story_runner/story_runner.mojom.h"
#include "apps/mozart/lib/skia/skia_surface_holder.h"
#include "apps/mozart/lib/view_framework/base_view.h"
#include "apps/mozart/services/views/interfaces/view_provider.mojom.h"
#include "apps/mozart/services/views/interfaces/view_token.mojom.h"
#include "lib/ftl/logging.h"
#include "mojo/public/cpp/application/application_impl_base.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/run_application.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_handle.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/system/macros.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"

namespace {

constexpr uint32_t kContentImageResourceId = 1;
constexpr uint32_t kRootNodeId = mozart::kSceneRootNodeId;
constexpr float kSpeed = 0.25f;
constexpr char kValueLabel[] = "value";
constexpr char kSenderLabel[] = "sender";

using document_store::Document;

using mojo::ApplicationImplBase;
using mojo::Binding;
using mojo::ConnectionContext;
using mojo::InterfaceHandle;
using mojo::InterfacePtr;
using mojo::InterfaceRequest;
using mojo::Map;
using mojo::ServiceProviderImpl;
using mojo::Shell;
using mojo::StrongBinding;
using mojo::StrongBindingSet;
using mojo::String;
using mojo::StructPtr;

using modular::Link;
using modular::LinkChanged;
using modular::Module;
using modular::ModuleClient;
using modular::ModuleWatcher;
using modular::DocumentEditor;
using modular::Session;

// Implementation of the LinkChanged service that forwards each document
// changed in one Link instance to a second Link instance.
class LinkConnection : public LinkChanged {
 public:
  LinkConnection(InterfacePtr<Link>& src, InterfacePtr<Link>& dst)
      : src_binding_(this), src_(src), dst_(dst) {
    InterfaceHandle<LinkChanged> watcher;
    src_binding_.Bind(GetProxy(&watcher));
    src_->Watch(std::move(watcher));
  }

  void Notify(StructPtr<Document> doc) override {
    FTL_LOG(INFO) << "LinkConnection::Notify() "
                  << DocumentEditor::ToString(doc);
    dst_->AddDocument(std::move(doc));
  }

 private:
  Binding<LinkChanged> src_binding_;
  InterfacePtr<Link>& src_;
  InterfacePtr<Link>& dst_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(LinkConnection);
};

// Implementation of the LinkChanged service that just reports every
// document changed in the given Link.
class LinkMonitor : public LinkChanged {
 public:
  LinkMonitor(const std::string tag, InterfacePtr<Link>& link)
      : binding_(this), tag_(tag) {
    InterfaceHandle<LinkChanged> watcher;
    binding_.Bind(GetProxy(&watcher));
    link->WatchAll(std::move(watcher));
  }

  void Notify(StructPtr<Document> doc) override {
    FTL_LOG(INFO) << "LinkMonitor::Notify()";
  }

 private:
  Binding<LinkChanged> binding_;
  const std::string tag_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(LinkMonitor);
};

class ModuleMonitor : public ModuleWatcher {
 public:
  ModuleMonitor(InterfacePtr<ModuleClient>& module_client,
                InterfacePtr<Session>& session)
      : binding_(this), session_(session) {
    InterfaceHandle<ModuleWatcher> watcher;
    binding_.Bind(GetProxy(&watcher));
    module_client->Watch(std::move(watcher));
  }

  void Done() override { session_->Done(); }

 private:
  Binding<ModuleWatcher> binding_;
  InterfacePtr<Session>& session_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(ModuleMonitor);
};

// Module implementation that acts as a recipe. It implements both
// Module and the LinkChanged observer of its own Link.
class RecipeImpl : public Module, public LinkChanged, public mozart::BaseView {
 public:
  RecipeImpl(mojo::InterfaceHandle<mojo::ApplicationConnector> app_connector,
             InterfaceRequest<Module> module_request,
             mojo::InterfaceRequest<mozart::ViewOwner> view_owner_request)
      : BaseView(app_connector.Pass(),
                 view_owner_request.Pass(),
                 "Spinning Square"),
        module_binding_(this, std::move(module_request)),
        watcher_binding_(this) {
    FTL_LOG(INFO) << "RecipeImpl";
  }
  ~RecipeImpl() override { FTL_LOG(INFO) << "~RecipeImpl"; }

  void Initialize(InterfaceHandle<Session> session,
                  InterfaceHandle<Link> link) override {
    FTL_LOG(INFO) << "RecipeImpl::Initialize()";

    // TODO(mesch): Good illustration of the remaining issue to
    // restart a session: How does this code look like when the
    // Session is not new, but already contains existing Modules and
    // Links from the previous execution that is continued here?

    session_.Bind(session.Pass());
    link_.Bind(link.Pass());

    InterfaceHandle<LinkChanged> watcher;
    watcher_binding_.Bind(&watcher);
    link_->Watch(std::move(watcher));

    session_->CreateLink(GetProxy(&module1_link_));
    session_->CreateLink(GetProxy(&module2_link_));

    InterfaceHandle<Link> module1_link_handle;  // To pass to StartModule().
    module1_link_->Dup(GetProxy(&module1_link_handle));

    InterfaceHandle<Link> module2_link_handle;  // To pass to StartModule().
    module2_link_->Dup(GetProxy(&module2_link_handle));

    FTL_LOG(INFO) << "recipe start module module1";
    session_->StartModule("mojo:example_module1",
                          std::move(module1_link_handle), GetProxy(&module1_),
                          GetProxy(&module1_view_));

    FTL_LOG(INFO) << "recipe start module module2";
    session_->StartModule("mojo:example_module2",
                          std::move(module2_link_handle), GetProxy(&module2_),
                          GetProxy(&module2_view_));

    monitors_.emplace_back(new LinkMonitor("module1", module1_link_));
    monitors_.emplace_back(new LinkMonitor("module2", module2_link_));

    connections_.emplace_back(new LinkConnection(module1_link_, module2_link_));
    connections_.emplace_back(new LinkConnection(module2_link_, module1_link_));

    module_monitors_.emplace_back(new ModuleMonitor(module1_, session_));
    module_monitors_.emplace_back(new ModuleMonitor(module2_, session_));

    // This must come last, otherwise we get a notification of our own
    // write because of the "send initial values" code.
    DocumentEditor doc("http://domokit.org/doc/1");
    doc.AddProperty(kValueLabel, DocumentEditor::NewIntValue(1));
    doc.AddProperty(kSenderLabel, DocumentEditor::NewStringValue("RecipeImpl"));
    module1_link_->AddDocument(doc.TakeDocument());
  }

  void Notify(StructPtr<Document> doc) override {
    FTL_LOG(INFO) << "RecipeImpl::Notify()";
  }

 private:
  // Copied from
  // https://fuchsia.googlesource.com/mozart/+/master/examples/spinning_square/spinning_square.cc
  // |BaseView|:
  void OnDraw() override {
    FTL_DCHECK(properties());
    auto update = mozart::SceneUpdate::New();
    const mojo::Size& size = *properties()->view_layout->size;
    if (size.width > 0 && size.height > 0) {
      mojo::RectF bounds;
      bounds.width = size.width;
      bounds.height = size.height;
      mozart::SkiaSurfaceHolder surface_holder(size);
      DrawContent(surface_holder.surface()->getCanvas(), size);
      auto content_resource = mozart::Resource::New();
      content_resource->set_image(mozart::ImageResource::New());
      content_resource->get_image()->image = surface_holder.TakeImage();
      update->resources.insert(kContentImageResourceId,
                               content_resource.Pass());
      auto root_node = mozart::Node::New();
      root_node->op = mozart::NodeOp::New();
      root_node->op->set_image(mozart::ImageNodeOp::New());
      root_node->op->get_image()->content_rect = bounds.Clone();
      root_node->op->get_image()->image_resource_id = kContentImageResourceId;
      update->nodes.insert(kRootNodeId, root_node.Pass());
    } else {
      auto root_node = mozart::Node::New();
      update->nodes.insert(kRootNodeId, root_node.Pass());
    }
    scene()->Update(update.Pass());
    scene()->Publish(CreateSceneMetadata());
    Invalidate();
  }
  void DrawContent(SkCanvas* canvas, const mojo::Size& size) {
    canvas->clear(SK_ColorBLUE);
    canvas->translate(size.width / 2, size.height / 2);
    float t =
        fmod(frame_tracker().frame_info().frame_time * 0.000001f * kSpeed, 1.f);
    canvas->rotate(360.f * t);
    SkPaint paint;
    paint.setColor(0xFFFF00FF);
    paint.setAntiAlias(true);
    float d = std::min(size.width, size.height) / 4;
    canvas->drawRect(SkRect::MakeLTRB(-d, -d, d, d), paint);
    canvas->flush();
  }

  StrongBinding<Module> module_binding_;
  StrongBinding<LinkChanged> watcher_binding_;

  InterfacePtr<Link> link_;
  InterfacePtr<Session> session_;

  InterfacePtr<ModuleClient> module1_;
  InterfacePtr<Link> module1_link_;

  InterfacePtr<ModuleClient> module2_;
  InterfacePtr<Link> module2_link_;

  std::vector<std::unique_ptr<LinkConnection>> connections_;
  std::vector<std::unique_ptr<LinkMonitor>> monitors_;
  std::vector<std::unique_ptr<ModuleMonitor>> module_monitors_;

  InterfacePtr<mozart::ViewOwner> module1_view_;
  InterfacePtr<mozart::ViewOwner> module2_view_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RecipeImpl);
};

}  // namespace

MojoResult MojoMain(MojoHandle application_request) {
  modular::SingleServiceViewApp<modular::Module, RecipeImpl> app;
  return mojo::RunApplication(application_request, &app);
}