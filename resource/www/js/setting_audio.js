$(function () {
  $("#audio_reset_sectret").click(function () {
    $.get('/cgi/audio_reset_sectret.cgi', function (data, status) {
      alert("result: " + status);
      func_get_audio_cfg();
    });
  });

  $("#audio_in_start").click(function () {
	var value = document.getElementById("audio_in_name").value
    $.get('/cgi/audio_record_sectret.cgi?in_name=' + value, function (data, status) {
		alert("result: " + status);
    });
  });

  $("#audio_out_start").click(function () {
	var value = document.getElementById("audio_out_name").value
    if (value == "") {
      alert("please input path");
      return;
    }
    $.get('/cgi/audio_record_sectret.cgi?out_name=' + value, function (data, status) {
		alert("result: " + status);
    });
  }); 

  $("#audio_in_stop").click(function () {
    $.get('/cgi/audio_record_sectret.cgi?in_stop', function (data, status) {
		alert("result: " + status);
    });
  }); 

  $("#audio_out_stop").click(function () {
	var value = document.getElementById("audio_out_name").value
	if (value == "") {
      alert("please input path");
      return;
    }
    $.get('/cgi/audio_record_sectret.cgi?out_stop', function (data, status) {
		alert("result: " + status);
    });
  }); 

  func_get_audio_cfg();

  $("#audio_save_sectret").click(function () {
      // param check
    console.log('audio save buttion click!! ...');
    if (document.getElementById("main_audio_enable").value == "1") {
      if (document.getElementById("ip_name").value == "") {
        alert("please input target host ip");
        return;
      }

      if (document.getElementById("port_name").value == "") {
        alert("please input port name");
        return;
      }
    }

    var stream_cfg_json = {};
    stream_cfg_json['main_enabled'] = document.getElementById("main_audio_enable").value;
    stream_cfg_json['ip'] = document.getElementById("ip_name").value;
    stream_cfg_json['port'] = document.getElementById("port_name").value;
    stream_cfg_json['in_slider'] = document.getElementById("audio_in_slider").value;
    stream_cfg_json['out_slider'] = document.getElementById("audio_out_slider").value;
    stream_cfg_json['amplifier_enable'] = document.getElementById("audio_amplifier_enable").value;
    stream_cfg_json['codec_select'] = document.getElementById("audio_codec_select").value;
    stream_cfg_json['samplerate_select'] = document.getElementById("audio_samplerate_select").value;
    stream_cfg_json['anr'] = document.getElementById("audio_anr_enable").value;
    stream_cfg_json['agc'] = document.getElementById("audio_agc_enable").value;

    stream_cfg_json['save_sectret'] = document.getElementById("audio_save_sectret").value;
    stream_cfg_json['reset_sectret'] = document.getElementById("audio_reset_sectret").value;

    var json = JSON.stringify(stream_cfg_json);
    $.get('/cgi/set_audio_cfg.cgi?' + json, function (data, status) {
      alert("result: " + status);
    });
  });
})

function func_get_audio_cfg() {
  $.get('/cgi/get_audio_cfg.cgi', function (data, status) {
    var obj = JSON.parse(data, function (key, value) {
      return value;
    });

    console.log(obj);
    $("#main_audio_enable").val(obj.main_enabled);
    $("#ip_name").val(obj.ip);
    $("#port_name").val(obj.port);
    $("#audio_in_slider").slider("setValue", obj.in_slider);
    $("#audio_in_slider_label").text(obj.in_slider);
    $("#audio_out_slider").slider("setValue", obj.out_slider);	
    $("#audio_out_slider_label").text(obj.out_slider);
    $("#audio_amplifier_enable").val(obj.amplifier_enable);
    $("#audio_codec_select").val(obj.codec_select);
    $("#audio_samplerate_select").val(obj.samplerate_select);
    $("#audio_anr_enable").val(obj.anr);
    $("#audio_agc_enable").val(obj.agc);
	
	$("#audio_in_name").val(obj.in_name);
	$("#audio_out_name").val(obj.out_name);
	
    $("#audio_save_sectret").val(obj.save_sectret);
    $("#audio_reset_sectret").val(obj.reset_sectret);
  })
}
