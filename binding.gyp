{
  "targets": [
    {
      "target_name": "socket-calls",
      "sources": [ "src/socket_calls.cc" ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags_cc": [ "-std=gnu++1z", "-fvisibility=hidden" ],
      "include_dirs": [
      	"<!@(node -p \"require('node-addon-api').include\")"
      ]
    }
  ]
}
