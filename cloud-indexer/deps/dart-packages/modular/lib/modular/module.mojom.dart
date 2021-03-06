// WARNING: DO NOT EDIT. This file was generated by a program.
// See $MOJO_SDK/tools/bindings/mojom_bindings_generator.py.

library module_mojom;
import 'dart:async';
import 'package:mojo/bindings.dart' as bindings;
import 'package:mojo/core.dart' as core;
import 'package:mojo/mojo/bindings/types/service_describer.mojom.dart' as service_describer;
import 'package:modular/modular/graph.mojom.dart' as graph_mojom;



class _ModuleOnInitializeParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(32, 0)
  ];
  graph_mojom.GraphInterface moduleGraph = null;
  Map<String, String> shorthandTable = null;
  List<String> jsonSchemas = null;

  _ModuleOnInitializeParams() : super(kVersions.last.size);

  _ModuleOnInitializeParams.init(
    graph_mojom.GraphInterface this.moduleGraph, 
    Map<String, String> this.shorthandTable, 
    List<String> this.jsonSchemas
  ) : super(kVersions.last.size);

  static _ModuleOnInitializeParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static _ModuleOnInitializeParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    _ModuleOnInitializeParams result = new _ModuleOnInitializeParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.moduleGraph = decoder0.decodeServiceInterface(8, false, graph_mojom.GraphProxy.newFromEndpoint);
    }
    if (mainDataHeader.version >= 0) {
      
      var decoder1 = decoder0.decodePointer(16, false);
      {
        decoder1.decodeDataHeaderForMap();
        List<String> keys0;
        List<String> values0;
        {
          
          var decoder2 = decoder1.decodePointer(bindings.ArrayDataHeader.kHeaderSize, false);
          {
            var si2 = decoder2.decodeDataHeaderForPointerArray(bindings.kUnspecifiedArrayLength);
            keys0 = new List<String>(si2.numElements);
            for (int i2 = 0; i2 < si2.numElements; ++i2) {
              
              keys0[i2] = decoder2.decodeString(bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i2, false);
            }
          }
        }
        {
          
          var decoder2 = decoder1.decodePointer(bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize, false);
          {
            var si2 = decoder2.decodeDataHeaderForPointerArray(keys0.length);
            values0 = new List<String>(si2.numElements);
            for (int i2 = 0; i2 < si2.numElements; ++i2) {
              
              values0[i2] = decoder2.decodeString(bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i2, false);
            }
          }
        }
        result.shorthandTable = new Map<String, String>.fromIterables(
            keys0, values0);
      }
    }
    if (mainDataHeader.version >= 0) {
      
      var decoder1 = decoder0.decodePointer(24, false);
      {
        var si1 = decoder1.decodeDataHeaderForPointerArray(bindings.kUnspecifiedArrayLength);
        result.jsonSchemas = new List<String>(si1.numElements);
        for (int i1 = 0; i1 < si1.numElements; ++i1) {
          
          result.jsonSchemas[i1] = decoder1.decodeString(bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i1, false);
        }
      }
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    try {
      encoder0.encodeInterface(moduleGraph, 8, false);
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "moduleGraph of struct _ModuleOnInitializeParams: $e";
      rethrow;
    }
    try {
      if (shorthandTable == null) {
        encoder0.encodeNullPointer(16, false);
      } else {
        var encoder1 = encoder0.encoderForMap(16);
        var keys0 = shorthandTable.keys.toList();
        var values0 = shorthandTable.values.toList();
        
        {
          var encoder2 = encoder1.encodePointerArray(keys0.length, bindings.ArrayDataHeader.kHeaderSize, bindings.kUnspecifiedArrayLength);
          for (int i1 = 0; i1 < keys0.length; ++i1) {
            encoder2.encodeString(keys0[i1], bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i1, false);
          }
        }
        
        {
          var encoder2 = encoder1.encodePointerArray(values0.length, bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize, bindings.kUnspecifiedArrayLength);
          for (int i1 = 0; i1 < values0.length; ++i1) {
            encoder2.encodeString(values0[i1], bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i1, false);
          }
        }
      }
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "shorthandTable of struct _ModuleOnInitializeParams: $e";
      rethrow;
    }
    try {
      if (jsonSchemas == null) {
        encoder0.encodeNullPointer(24, false);
      } else {
        var encoder1 = encoder0.encodePointerArray(jsonSchemas.length, 24, bindings.kUnspecifiedArrayLength);
        for (int i0 = 0; i0 < jsonSchemas.length; ++i0) {
          encoder1.encodeString(jsonSchemas[i0], bindings.ArrayDataHeader.kHeaderSize + bindings.kPointerSize * i0, false);
        }
      }
    } on bindings.MojoCodecError catch(e) {
      e.message = "Error encountered while encoding field "
          "jsonSchemas of struct _ModuleOnInitializeParams: $e";
      rethrow;
    }
  }

  String toString() {
    return "_ModuleOnInitializeParams("
           "moduleGraph: $moduleGraph" ", "
           "shorthandTable: $shorthandTable" ", "
           "jsonSchemas: $jsonSchemas" ")";
  }

  Map toJson() {
    throw new bindings.MojoCodecError(
        'Object containing handles cannot be encoded to JSON.');
  }
}

const int _moduleMethodOnInitializeName = 0;

class _ModuleServiceDescription implements service_describer.ServiceDescription {
  void getTopLevelInterface(Function responder) {
    responder(null);
  }

  void getTypeDefinition(String typeKey, Function responder) {
    responder(null);
  }

  void getAllTypeDefinitions(Function responder) {
    responder(null);
  }
}

abstract class Module {
  static const String serviceName = "modular::Module";

  static service_describer.ServiceDescription _cachedServiceDescription;
  static service_describer.ServiceDescription get serviceDescription {
    if (_cachedServiceDescription == null) {
      _cachedServiceDescription = new _ModuleServiceDescription();
    }
    return _cachedServiceDescription;
  }

  static ModuleProxy connectToService(
      bindings.ServiceConnector s, String url, [String serviceName]) {
    ModuleProxy p = new ModuleProxy.unbound();
    String name = serviceName ?? Module.serviceName;
    if ((name == null) || name.isEmpty) {
      throw new core.MojoApiError(
          "If an interface has no ServiceName, then one must be provided.");
    }
    s.connectToService(url, p, name);
    return p;
  }
  void onInitialize(graph_mojom.GraphInterface moduleGraph, Map<String, String> shorthandTable, List<String> jsonSchemas);
}

abstract class ModuleInterface
    implements bindings.MojoInterface<Module>,
               Module {
  factory ModuleInterface([Module impl]) =>
      new ModuleStub.unbound(impl);

  factory ModuleInterface.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint,
      [Module impl]) =>
      new ModuleStub.fromEndpoint(endpoint, impl);

  factory ModuleInterface.fromMock(
      Module mock) =>
      new ModuleProxy.fromMock(mock);
}

abstract class ModuleInterfaceRequest
    implements bindings.MojoInterface<Module>,
               Module {
  factory ModuleInterfaceRequest() =>
      new ModuleProxy.unbound();
}

class _ModuleProxyControl
    extends bindings.ProxyMessageHandler
    implements bindings.ProxyControl<Module> {
  Module impl;

  _ModuleProxyControl.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) : super.fromEndpoint(endpoint);

  _ModuleProxyControl.fromHandle(
      core.MojoHandle handle) : super.fromHandle(handle);

  _ModuleProxyControl.unbound() : super.unbound();

  String get serviceName => Module.serviceName;

  void handleResponse(bindings.ServiceMessage message) {
    switch (message.header.type) {
      default:
        proxyError("Unexpected message type: ${message.header.type}");
        close(immediate: true);
        break;
    }
  }

  @override
  String toString() {
    var superString = super.toString();
    return "_ModuleProxyControl($superString)";
  }
}

class ModuleProxy
    extends bindings.Proxy<Module>
    implements Module,
               ModuleInterface,
               ModuleInterfaceRequest {
  ModuleProxy.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint)
      : super(new _ModuleProxyControl.fromEndpoint(endpoint));

  ModuleProxy.fromHandle(core.MojoHandle handle)
      : super(new _ModuleProxyControl.fromHandle(handle));

  ModuleProxy.unbound()
      : super(new _ModuleProxyControl.unbound());

  factory ModuleProxy.fromMock(Module mock) {
    ModuleProxy newMockedProxy =
        new ModuleProxy.unbound();
    newMockedProxy.impl = mock;
    return newMockedProxy;
  }

  static ModuleProxy newFromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) {
    assert(endpoint.setDescription("For ModuleProxy"));
    return new ModuleProxy.fromEndpoint(endpoint);
  }


  void onInitialize(graph_mojom.GraphInterface moduleGraph, Map<String, String> shorthandTable, List<String> jsonSchemas) {
    if (impl != null) {
      impl.onInitialize(moduleGraph, shorthandTable, jsonSchemas);
      return;
    }
    if (!ctrl.isBound) {
      ctrl.proxyError("The Proxy is closed.");
      return;
    }
    var params = new _ModuleOnInitializeParams();
    params.moduleGraph = moduleGraph;
    params.shorthandTable = shorthandTable;
    params.jsonSchemas = jsonSchemas;
    ctrl.sendMessage(params,
        _moduleMethodOnInitializeName);
  }
}

class _ModuleStubControl
    extends bindings.StubMessageHandler
    implements bindings.StubControl<Module> {
  Module _impl;

  _ModuleStubControl.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint, [Module impl])
      : super.fromEndpoint(endpoint, autoBegin: impl != null) {
    _impl = impl;
  }

  _ModuleStubControl.fromHandle(
      core.MojoHandle handle, [Module impl])
      : super.fromHandle(handle, autoBegin: impl != null) {
    _impl = impl;
  }

  _ModuleStubControl.unbound([this._impl]) : super.unbound();

  String get serviceName => Module.serviceName;



  void handleMessage(bindings.ServiceMessage message) {
    if (bindings.ControlMessageHandler.isControlMessage(message)) {
      bindings.ControlMessageHandler.handleMessage(
          this, 0, message);
      return;
    }
    if (_impl == null) {
      throw new core.MojoApiError("$this has no implementation set");
    }
    switch (message.header.type) {
      case _moduleMethodOnInitializeName:
        var params = _ModuleOnInitializeParams.deserialize(
            message.payload);
        _impl.onInitialize(params.moduleGraph, params.shorthandTable, params.jsonSchemas);
        break;
      default:
        throw new bindings.MojoCodecError("Unexpected message name");
        break;
    }
  }

  Module get impl => _impl;
  set impl(Module d) {
    if (d == null) {
      throw new core.MojoApiError("$this: Cannot set a null implementation");
    }
    if (isBound && (_impl == null)) {
      beginHandlingEvents();
    }
    _impl = d;
  }

  @override
  void bind(core.MojoMessagePipeEndpoint endpoint) {
    super.bind(endpoint);
    if (!isOpen && (_impl != null)) {
      beginHandlingEvents();
    }
  }

  @override
  String toString() {
    var superString = super.toString();
    return "_ModuleStubControl($superString)";
  }

  int get version => 0;
}

class ModuleStub
    extends bindings.Stub<Module>
    implements Module,
               ModuleInterface,
               ModuleInterfaceRequest {
  ModuleStub.unbound([Module impl])
      : super(new _ModuleStubControl.unbound(impl));

  ModuleStub.fromEndpoint(
      core.MojoMessagePipeEndpoint endpoint, [Module impl])
      : super(new _ModuleStubControl.fromEndpoint(endpoint, impl));

  ModuleStub.fromHandle(
      core.MojoHandle handle, [Module impl])
      : super(new _ModuleStubControl.fromHandle(handle, impl));

  static ModuleStub newFromEndpoint(
      core.MojoMessagePipeEndpoint endpoint) {
    assert(endpoint.setDescription("For ModuleStub"));
    return new ModuleStub.fromEndpoint(endpoint);
  }


  void onInitialize(graph_mojom.GraphInterface moduleGraph, Map<String, String> shorthandTable, List<String> jsonSchemas) {
    return impl.onInitialize(moduleGraph, shorthandTable, jsonSchemas);
  }
}



