$(function () {
  func_get_stream_cfg();
  func_get_record();

  $("#record_status").change(function () {
    $.get('/cgi/set_record_sectret?start=' + this.value, function (data, status) {
      alert("result: " + status);
    });
  });

  $("#reset_stream_cfg").click(function () {
    $.get('/cgi/reset_stream_cfg.cgi', function (data, status) {
      alert("result: " + status);
      func_get_stream_cfg();
    });
  });

  $("#save_stream_cfg").click(function () {
    // param check
    if (document.getElementById("main_stream_bitrate").value == "") {
      alert("please input main stream bitrate");
      return;
    }

    if (document.getElementById("sub_stream_bitrate").value == "") {
      alert("please input sub stream bitrate");
      return;
    }

    var stream_cfg_json = {};
    stream_cfg_json['main_enabled'] = document.getElementById("main_stream_enabled").value;
    stream_cfg_json['main_codec'] = document.getElementById("main_stream_codec").value;
    stream_cfg_json['main_resolution'] = document.getElementById("main_stream_resolution").value;
    stream_cfg_json['main_fps'] =  $("#main_stream_fps").val();
    stream_cfg_json['main_profile'] = document.getElementById("main_stream_profile").value;
    stream_cfg_json['main_rc'] = document.getElementById("main_stream_rc").value;
    stream_cfg_json['main_bitrate'] = document.getElementById("main_stream_bitrate").value;

    stream_cfg_json['sub_enabled'] = document.getElementById("sub_stream_enabled").value;
    stream_cfg_json['sub_codec'] = document.getElementById("sub_stream_codec").value;
    stream_cfg_json['sub_resolution'] = document.getElementById("sub_stream_resolution").value;
    stream_cfg_json['sub_fps'] = $("#sub_stream_fps").val();
    stream_cfg_json['sub_profile'] = document.getElementById("sub_stream_profile").value;
    stream_cfg_json['sub_rc'] = document.getElementById("sub_stream_rc").value;
    stream_cfg_json['sub_bitrate'] = document.getElementById("sub_stream_bitrate").value;

    var stream_json = JSON.stringify(stream_cfg_json);
    $.get('/cgi/set_stream_cfg.cgi?' + stream_json, function (data, status) {
      alert("result: " + status);
    });
  });

  $("#record_check").click(function () {
    func_record_check();
  });

  $("#replay_start").click(function () {
    func_replay_start();
  });

  $("#replay_stop").click(function () {
    func_replay_stop();
  });

  func_get_roi_cfg();
  $("#save_roi_cfg").click(function () {
    func_set_roi_cfg();
  });
})

function func_get_stream_cfg() {
  $.get('/cgi/get_stream_cfg.cgi', function (data, status) {
    var obj = JSON.parse(data, function (key, value) {
      return value;
    });

    console.log(obj);
    $("#main_stream_enabled").val(obj.main_enabled);
    $("#main_stream_codec").val(obj.main_codec);
    $("#main_stream_resolution").val(obj.main_resolution);
    $("#main_stream_fps").val(obj.main_fps);
    $("#main_stream_profile").val(obj.main_profile);
    $("#main_stream_rc").val(obj.main_rc);
    $("#main_stream_bitrate").val(obj.main_bitrate);

    $("#sub_stream_enabled").val(obj.sub_enabled);
    $("#sub_stream_codec").val(obj.sub_codec);
    $("#sub_stream_resolution").val(obj.sub_resolution);
    $("#sub_stream_fps").val(obj.sub_fps);
    $("#sub_stream_profile").val(obj.sub_profile);
    $("#sub_stream_rc").val(obj.sub_rc);
    $("#sub_stream_bitrate").val(obj.sub_bitrate);
  })
}

function func_record_check() {
	var value = document.getElementById("record_date").value
    $.get('/cgi/set_record_sectret?record_date=' + value, function (data, status) {
      alert("result: " + status);
    });
	func_get_record();
}

function func_get_record() {
	$.get('/cgi/get_record_sectret.cgi', function (data, status) {
    var obj = JSON.parse(data, function (key, value) {
      return value;
    });

    console.log(obj);
    $("#record_segment").val(obj.segment);
	$("#record_status").val(obj.start);
  })
}

function func_replay_start() {
	var value = document.getElementById("replay_date").value
    $.get('/cgi/start_replay_sectret?replay_date=' + value, function (data, status) {
      alert("result: " + status);
    });
}

function func_replay_stop() {
    $.get('/cgi/stop_replay_sectret.cgi', function (data, status) {
      alert("result: " + status);
    });
}

function func_set_roi_cfg() {
  var roi_settings = {};
  for (i = 0; i < 8; i++) { // todo
    x = i;
    roi_settings['roi_enable'+String(x)] = document.getElementById('roi_enable'+String(x)).value;
	
	if (document.getElementById('roi_venc'+String(x)).value == "") {
      alert("please input roi_venc chn");
      return;
    }
	roi_settings['roi_venc'+String(x)] = document.getElementById('roi_venc'+String(x)).value;
	if (roi_settings['roi_enable'+String(x)] == 0) {
	  continue;
	}
    roi_settings['roi_abs_qp'+String(x)] = document.getElementById('roi_abs_qp'+String(x)).value;
    if (document.getElementById('roi_abs_qp'+String(x)).value == "") {
      alert("please input roi_abs_qp bitrate");
      return;
    }
    roi_settings['roi_abs_qp'+String(x)] = document.getElementById('roi_abs_qp'+String(x)).value;

    if (document.getElementById('roi_qp'+String(x)).value == "") {
      alert("please input roi_qp bitrate");
      return;
    }
    roi_settings['roi_qp'+String(x)] = document.getElementById('roi_qp'+String(x)).value;

    if (document.getElementById('roi_x'+String(x)).value == "") {
      alert("please input roi_x bitrate");
      return;
    }
    roi_settings['roi_x'+String(x)] = document.getElementById('roi_x'+String(x)).value;

    if (document.getElementById('roi_y'+String(x)).value == "") {
      alert("please input roi_y bitrate");
      return;
    }
    roi_settings['roi_y'+String(x)] = document.getElementById('roi_y'+String(x)).value;

    if (document.getElementById('roi_width'+String(x)).value == "") {
      alert("please input roi_width bitrate");
      return;
    }
    roi_settings['roi_width'+String(x)] = document.getElementById('roi_width'+String(x)).value;

    if (document.getElementById('roi_hight'+String(x)).value == "") {
      alert("please input roi_hight bitrate");
      return;
    }
    roi_settings['roi_hight'+String(x)] = document.getElementById('roi_hight'+String(x)).value;
  }
  console.log(roi_settings);
  var roi_json = JSON.stringify(roi_settings);

  $.get('/cgi/set_roi_cfg.cgi?' + roi_json, function (data, status) {
    alert("result: " + status);
  });
}

function func_get_roi_cfg() {
  $.get('/cgi/get_roi_cfg.cgi', function (data, status) {
    console.log(status);
    var roi_obj = JSON.parse(data, function (key, value) {
      return value;
    });
    console.log(roi_obj);

    $("#roi_enable0").val(roi_obj.roi_enable0);
	$("#roi_venc0").val(roi_obj.roi_venc0);
	$("#roi_abs_qp0").val(roi_obj.roi_abs_qp0);
    $("#roi_qp0").val(roi_obj.roi_qp0);
    $("#roi_x0").val(roi_obj.roi_x0);
    $("#roi_y0").val(roi_obj.roi_y0);
    $("#roi_width0").val(roi_obj.roi_width0);
    $("#roi_hight0").val(roi_obj.roi_hight0);

    $("#roi_enable1").val(roi_obj.roi_enable1);
	$("#roi_venc1").val(roi_obj.roi_venc1);
    $("#roi_abs_qp1").val(roi_obj.roi_abs_qp1);
    $("#roi_qp1").val(roi_obj.roi_qp1);
    $("#roi_x1").val(roi_obj.roi_x1);
    $("#roi_y1").val(roi_obj.roi_y1);
    $("#roi_width1").val(roi_obj.roi_width1);
    $("#roi_hight1").val(roi_obj.roi_hight1);

    $("#roi_enable2").val(roi_obj.roi_enable2);
	$("#roi_venc2").val(roi_obj.roi_venc2);
    $("#roi_abs_qp2").val(roi_obj.roi_abs_qp2);
    $("#roi_qp2").val(roi_obj.roi_qp2);
    $("#roi_x2").val(roi_obj.roi_x2);
    $("#roi_y2").val(roi_obj.roi_y2);
    $("#roi_width2").val(roi_obj.roi_width2);
    $("#roi_hight2").val(roi_obj.roi_hight2);

    $("#roi_enable3").val(roi_obj.roi_enable3);
	$("#roi_venc3").val(roi_obj.roi_venc3);
    $("#roi_abs_qp3").val(roi_obj.roi_abs_qp3);
    $("#roi_qp3").val(roi_obj.roi_qp3);
    $("#roi_x3").val(roi_obj.roi_x3);
    $("#roi_y3").val(roi_obj.roi_y3);
    $("#roi_width3").val(roi_obj.roi_width3);
    $("#roi_hight3").val(roi_obj.roi_hight3);

    $("#roi_enable4").val(roi_obj.roi_enable4);
	$("#roi_venc4").val(roi_obj.roi_venc4);
    $("#roi_abs_qp4").val(roi_obj.roi_abs_qp4);
    $("#roi_qp4").val(roi_obj.roi_qp4);
    $("#roi_x4").val(roi_obj.roi_x4);
    $("#roi_y4").val(roi_obj.roi_y4);
    $("#roi_width4").val(roi_obj.roi_width4);
    $("#roi_hight4").val(roi_obj.roi_hight4);

    $("#roi_enable5").val(roi_obj.roi_enable5);
	$("#roi_venc5").val(roi_obj.roi_venc5);
    $("#roi_abs_qp5").val(roi_obj.roi_abs_qp5);
    $("#roi_qp5").val(roi_obj.roi_qp5);
    $("#roi_x5").val(roi_obj.roi_x5);
    $("#roi_y5").val(roi_obj.roi_y5);
    $("#roi_width5").val(roi_obj.roi_width5);
    $("#roi_hight5").val(roi_obj.roi_hight5);

    $("#roi_enable6").val(roi_obj.roi_enable6);
	$("#roi_venc6").val(roi_obj.roi_venc6);
    $("#roi_abs_qp6").val(roi_obj.roi_abs_qp6);
    $("#roi_qp6").val(roi_obj.roi_qp6);
    $("#roi_x6").val(roi_obj.roi_x6);
    $("#roi_y6").val(roi_obj.roi_y6);
    $("#roi_width6").val(roi_obj.roi_width6);
    $("#roi_hight6").val(roi_obj.roi_hight6);

    $("#roi_enable7").val(roi_obj.roi_enable7);
	$("#roi_venc7").val(roi_obj.roi_venc7);
    $("#roi_abs_qp7").val(roi_obj.roi_abs_qp7);
    $("#roi_qp7").val(roi_obj.roi_qp7);
    $("#roi_x7").val(roi_obj.roi_x7);
    $("#roi_y7").val(roi_obj.roi_y7);
    $("#roi_width7").val(roi_obj.roi_width7);
    $("#roi_hight7").val(roi_obj.roi_hight7);
  });
}