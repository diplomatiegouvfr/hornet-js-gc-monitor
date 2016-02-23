{
  "targets": [
    {
      "target_name": "hornet-js-gc-monitor",
      "sources": [ "src/hornet-js-gc-monitor.cc" ],
      "cflags_cc" : ["-fexceptions"],
      "include_dirs" : [
          "<(node_root_dir)/deps/cares/include",
          "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}
