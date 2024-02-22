mergeInto(LibraryManager.library, {
  pin_: function(v) {},

  clear_stack__deps: ['emscripten_stack_get_current', 'emscripten_stack_get_end'],
  clear_stack: function() {
    // Clear the unused portion of the stack. This is because even though the exact stack range
    // is scanned when marking, not all bytes are cleared on the stack but there are holes left behind.
    // In normal use this won't be a problem, since as code traverses a lot of functions, these holes
    // will be cleared; but in small unit test code it can make the stack scanning mistakenly find
    // pointers from a previous call. So clear the unused part of the stack when returning from a
    // nested function.
    // Add +16 to avoid stomping on the stack cookie.
    HEAPU8.fill(0, _emscripten_stack_get_end()+16, _emscripten_stack_get_current());
  },

  call_from_js_p__deps: ['clear_stack', '$UTF8ToString'],
  call_from_js_p: function(func) {
    var ret = {{{ makeDynCall('i', 'func') }}}();
    _clear_stack();
    return ret;
  },

  call_from_js_v__deps: ['clear_stack', '$UTF8ToString'],
  call_from_js_v: function(func) {
    {{{ makeDynCall('v', 'func') }}}();
    _clear_stack();
  }
});
