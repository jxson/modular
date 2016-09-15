// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:io' show HttpStatus;

import 'package:logging/logging.dart';
import 'package:notification_handler/index_updater.dart';
import 'package:notification_handler/message.dart';
import 'package:shelf/shelf.dart' as shelf;

final Logger _logger = new Logger('notification_handler.request_handler');

const String _subscriptionName = 'projects/google.com:modular-cloud-indexer/'
    'subscriptions/notification-handler';

/// Handles a request from the subscription specified by [subscriptionName].
///
/// Currently, we assume that there is no notion of deleting a module: once a
/// manifest is removed, we have no means of determining the URL the module is
/// indexed against.
Future<shelf.Response> requestHandler(shelf.Request request,
    {IndexUpdater indexUpdater,
    String subscriptionName: _subscriptionName}) async {
  // In the case that an IndexUpdater is not provided, we use the one registered
  // to the current service scope.
  indexUpdater ??= indexUpdaterService;

  // TODO(victorkwan): Provide authentication checks once these are available
  // on Managed VMs: https://code.google.com/p/cloud-pubsub/issues/detail?id=32
  if (request.method != 'POST') {
    return new shelf.Response.notFound(null);
  }

  final String pushRequest = await request.readAsString();
  final Message message = new Message(pushRequest);
  if (message == null) {
    _logger.info('Invalid message received.');
    return new shelf.Response(HttpStatus.BAD_REQUEST);
  }

  if (message.subscription != subscriptionName) {
    // Here, we respond OK to stop further requests from being sent.
    _logger.info('Invalid subscription: ${message.subscription}');
    return new shelf.Response.ok(null);
  }

  try {
    _logger.info('Starting manifest update.');
    await indexUpdater.update(message.data);
  } on AtomicUpdateFailureException {
    return new shelf.Response.internalServerError();
  } on CloudStorageFailureException {
    return new shelf.Response.internalServerError();
  } on ManifestException {
    // In the case that the manifest is malformed or non-existent, we should
    // not attempt to retry updating the index.
    return new shelf.Response.ok(null);
  }

  _logger.info('Finished manifest update.');
  return new shelf.Response.ok(null);
}