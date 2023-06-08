$(function () {
  var jmuxer = new JMuxer({
    node: 'ai_sub_player',
    mode: 'video',
    flushingTime: 40,
    fps: 25,
    clearBuffer: true,
    debug: false,
  });

  $.get('/cgi/get_ws_addr.cgi', function (data, status) {
    console.log("enter connectWS, addr: " + data);

    if (data == "ws://0.0.0.0:8000"){
      alert("只支持单路播放");
      return
    }

    var ws = new WebSocket(data);
    ws.binaryType = 'arraybuffer';

    ws.onopen = function (evt) {
      console.log('Connection open ...');
    };

    ws.onerror = function (evt) {
      console.log('WS error ' + evt.message);
    }

    ws.onmessage = function (evt) {
      jmuxer.feed({
        video: new Uint8Array(evt.data)
      });
    }
  });

  func_get_ai_cfg();

  $("#reset_ai_cfg").click(function () {
    $.get('/cgi/reset_stream_cfg.cgi', function (data, status) {
      alert("result: " + status);
      func_get_ai_cfg();
    });
  });

  $("#save_ai_cfg").click(function () {
    // param check
    var ai_cfg_json = {};

    ai_cfg_json['md_enable'] = document.getElementById("md_enabled").value;
    ai_cfg_json['pd_enable'] = document.getElementById("pd_enabled").value;
    ai_cfg_json['pd_intrusion_enable'] = document.getElementById("pd_intrusion_enabled").value;
    ai_cfg_json['md_threshold'] =  document.getElementById("md_threshold").value;
    ai_cfg_json['pd_threshold'] =  document.getElementById("pd_threshold").value;

    ai_cfg_json['region_x1'] =  document.getElementById("pd_privacy_x1").value;
    ai_cfg_json['region_y1'] =  document.getElementById("pd_privacy_y1").value;
    ai_cfg_json['region_x2'] =  document.getElementById("pd_privacy_x2").value;
    ai_cfg_json['region_y2'] =  document.getElementById("pd_privacy_y2").value;
    ai_cfg_json['region_x3'] =  document.getElementById("pd_privacy_x3").value;
    ai_cfg_json['region_y3'] =  document.getElementById("pd_privacy_y3").value;
    ai_cfg_json['region_x4'] =  document.getElementById("pd_privacy_x4").value;
    ai_cfg_json['region_y4'] =  document.getElementById("pd_privacy_y4").value;
    ai_cfg_json['region_x5'] =  document.getElementById("pd_privacy_x5").value;
    ai_cfg_json['region_y5'] =  document.getElementById("pd_privacy_y5").value;
    ai_cfg_json['region_x6'] =  document.getElementById("pd_privacy_x6").value;
    ai_cfg_json['region_y6'] =  document.getElementById("pd_privacy_y6").value;
    
    var ainfo_json = JSON.stringify(ai_cfg_json);
    $.get('/cgi/set_ai_info.cgi?' + ainfo_json, function (data, status) {
      alert("result: " + status);
    });

  });
  $("#ai_register").click(function () {
    var ai_face_register = {};
    ai_face_register['ai_name'] = document.getElementById("ai_name").value;
    var ainfo_json = JSON.stringify(ai_face_register);
    $.get('/cgi/register_ai_face.cgi?' + ainfo_json, function (data, status) {
      alert("result: " + status);
    });
  });
  $("#ai_delete").click(function () {
    var ai_face_register = {};
    ai_face_register['ai_name'] = document.getElementById("ai_name").value;
    var ainfo_json = JSON.stringify(ai_face_register);
    $.get('/cgi/unregister_ai_face.cgi?' + ainfo_json, function (data, status) {
      alert("result: " + status);
    });
  });
})

  function func_get_ai_cfg() {
    $.get('/cgi/get_ai_info.cgi', function (data, status) {
      var obj = JSON.parse(data, function (key, value) {
        return value;
      });
  
      console.log(obj);
      $("#smart_model").val(obj.ai_model);
      $("#md_enabled").val(obj.md_enable);
      $("#pd_enabled").val(obj.pd_enable);
      $("#pd_intrusion_enabled").val(obj.pd_intrusion_enable);
      $("#md_threshold").val(obj.md_threshold);
      $("#pd_threshold").val(obj.pd_threshold);

      $("#pd_privacy_x1").val(obj.region_x1);
      $("#pd_privacy_y1").val(obj.region_y1);
      $("#pd_privacy_x2").val(obj.region_x2);
      $("#pd_privacy_y2").val(obj.region_y2);
      $("#pd_privacy_x3").val(obj.region_x3);
      $("#pd_privacy_y3").val(obj.region_y3);
      $("#pd_privacy_x4").val(obj.region_x4);
      $("#pd_privacy_y4").val(obj.region_y4);
      $("#pd_privacy_x5").val(obj.region_x5);
      $("#pd_privacy_y5").val(obj.region_y5);
      $("#pd_privacy_x6").val(obj.region_x6);
      $("#pd_privacy_y6").val(obj.region_y6);
    })
  }