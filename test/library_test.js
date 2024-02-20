mergeInto(LibraryManager.library, {
  pin_: function(v) {},
  call_from_js_p: function(func) {
    return {{{ makeDynCall('v', 'func') }}}();
  },
  call_from_js_v: 'call_from_js_p'
});
