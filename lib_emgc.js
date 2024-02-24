mergeInto(LibraryManager.library, {
  $gc_fprintf_str: '=[0]',
  $gc_fprintf_regex: '=/%l*(u|d)/g',
  gc_fprintf__deps: ['$gc_fprintf_str', '$gc_fprintf_regex'
#if WASM_WORKERS
  , 'emscripten_wasm_worker_self_id'
#endif
  ],
  gc_fprintf: function(fd, format, varArgs) {
    var ch, val, i = format;
    gc_fprintf_str.length = 1; // Preserve first argument as a placeholder for format string.
    varArgs >>= 2; // Variadic arguments are always at minimum 4-byte aligned, so pre-shift here to save code size.
    while(HEAPU8[i]) {
      if (HEAPU8[i++] == 37 /*'%'*/) {
        while((ch = HEAPU8[i++])) {
          if (ch == 102 /*'f'*/ || (ch == 108 /*'l'*/ && HEAPU8[i] == ch)) {
            varArgs = (varArgs + 1) & -2; // align up to 8 byte boundary.
            val = HEAPU32[varArgs+1];
            gc_fprintf_str.push((ch == 108)
              ? HEAPU32[varArgs] + (HEAPU8[i+1] == 117 /*'u'*/ ? val : val|0) * 4294967296
              : HEAPF64[varArgs >> 1]);
            varArgs += 2;
            break;
          }
          if (ch > 57 /*'9'*/) { // assume it is a 'd', 'i' or 'u' (this also prevents runaway scans)
            val = HEAPU32[varArgs++];
            gc_fprintf_str.push(ch == 115 /*'s'*/
              ? UTF8ToString(val)
              : (ch == 117 /* 'u' */ ? val : val|0));
            break;
          }
        }
      }
    }
    // Swallow last newline since console.log() outputs full lines. Note: if one prints incomplete
    // lines, i.e. printf("something without newline"), a full line will still be printed.
    if (HEAPU8[i-1] == 10 /* \n */) --i;

    // Construct the string to print, but also replace all %u, %lld and %llu's with %d's, since console.log API doesn't
    // recognize them as a format specifier. (those were handled above)
    gc_fprintf_str[0] = (
#if TEXTDECODER == 2
        UTF8Decoder.decode(HEAPU8.subarray(format, i))
#else
        UTF8ArrayToString(HEAPU8, format, i-format)
#endif
        ).replace(gc_fprintf_regex, '%d');
#if WASM_WORKERS
    gc_fprintf_str[0] = `Thread ${_emscripten_wasm_worker_self_id()|0}: ${gc_fprintf_str[0]}`;
#endif
    console[(fd?'error':'log')](...gc_fprintf_str);
  },

  gc_log__deps: ['gc_fprintf'],
  gc_log: function(format, varArgs) {
    _gc_fprintf(0/*stdout*/, format, varArgs);
  },

  gc_loge__deps: ['gc_fprintf'],
  gc_loge: function(format, varArgs) {
    _gc_fprintf(1/*stderr*/, format, varArgs);
  }

});
