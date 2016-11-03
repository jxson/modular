// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/lib/app/service_provider_impl.h"

#include <utility>

namespace modular {

ServiceProviderImpl::ServiceProviderImpl() : binding_(this) {}

ServiceProviderImpl::ServiceProviderImpl(
    fidl::InterfaceRequest<ServiceProvider> request)
    : binding_(this) {
  if (request.is_pending())
    Bind(std::move(request));
}

ServiceProviderImpl::~ServiceProviderImpl() = default;

void ServiceProviderImpl::Bind(
    fidl::InterfaceRequest<ServiceProvider> request) {
  binding_.Bind(std::move(request));
}

void ServiceProviderImpl::Close() {
  if (binding_.is_bound())
    binding_.Close();
}

void ServiceProviderImpl::AddServiceForName(ServiceConnector connector,
                                            const std::string& service_name) {
  name_to_service_connector_[service_name] = std::move(connector);
}

void ServiceProviderImpl::RemoveServiceForName(
    const std::string& service_name) {
  auto it = name_to_service_connector_.find(service_name);
  if (it != name_to_service_connector_.end())
    name_to_service_connector_.erase(it);
}

void ServiceProviderImpl::ConnectToService(const fidl::String& service_name,
                                           mx::channel client_handle) {
  auto it = name_to_service_connector_.find(service_name);
  if (it != name_to_service_connector_.end())
    it->second(std::move(client_handle));
}

}  // namespace modular