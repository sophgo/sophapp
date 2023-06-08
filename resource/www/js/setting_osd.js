$(function () {
  document.getElementById("video_overlay").style.display = "none";

  func_get_osd_cfg();
  var resolution_size = func_osd_pos_check();

  var jmuxer = new JMuxer({
    node: 'sub_player',
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

  $("#osd_btn_save").click(function () {
    var osd_settings = {};

    console.log('click resolution.main_w=%d', resolution_size.main_w);
    console.log('click resolution.main_h=%d', resolution_size.main_h);
    console.log('click resolution.sub_w=%d', resolution_size.sub_w);
    console.log('click resolution.sub_h=%d', resolution_size.sub_h);
  
    // global
    osd_settings['global_switch'] = document.getElementById("switch_osd_global").value;
    // timestamp
    osd_settings['ts_switch'] = document.getElementById("switch_osd_time").value;
    osd_settings['ts_x'] = document.getElementById("osd_ts_x").value;
    osd_settings['ts_y'] = document.getElementById("osd_ts_y").value;
    osd_settings['ts_type'] = document.getElementById("osd_ts_type").value;
    osd_settings['ts_font_size'] = document.getElementById("osd_ts_font_size").value;
    // text1
    osd_settings['text1_switch'] = document.getElementById("switch_osd_text1").value;
    osd_settings['text1_x'] = document.getElementById("osd_text1_x").value;
    osd_settings['text1_y'] = document.getElementById("osd_text1_y").value;
    osd_settings['text1_font_size'] = document.getElementById("osd_text1_font_size").value;
    osd_settings['text1_color'] = document.getElementById("osd_text1_color").value;
    osd_settings['text1_content'] = document.getElementById("osd_text1_content").value;
    console.log(osd_settings['text1_content'].length);
    var txt1_len = osd_settings['text1_content'].length;
    // if (txt1_len * 24 > resolution_size.main_w) {
    if (txt1_len > 48) {
        alert('string count : ' + txt1_len + ' too long' + ' and Max count is 48');
        func_get_osd_cfg();
        return;
    }
    // text2
    osd_settings['text2_switch'] = document.getElementById("switch_osd_text2").value;
    osd_settings['text2_x'] = document.getElementById("osd_text2_x").value;
    osd_settings['text2_y'] = document.getElementById("osd_text2_y").value;
    osd_settings['text2_font_size'] = document.getElementById("osd_text2_font_size").value;
    osd_settings['text2_color'] = document.getElementById("osd_text2_color").value;
    osd_settings['text2_content'] = document.getElementById("osd_text2_content").value;
    console.log(osd_settings['text2_content'].length);
    var txt2_len = osd_settings['text2_content'].length;
    if (txt2_len > 48) {
        alert('string count : ' + txt2_len + ' too long' + ' and Max count is 48');
        func_get_osd_cfg();
        return;
    }
    // text3
    osd_settings['text3_switch'] = document.getElementById("switch_osd_text3").value;
    osd_settings['text3_x'] = document.getElementById("osd_text3_x").value;
    osd_settings['text3_y'] = document.getElementById("osd_text3_y").value;
    osd_settings['text3_font_size'] = document.getElementById("osd_text3_font_size").value;
    osd_settings['text3_color'] = document.getElementById("osd_text3_color").value;
    osd_settings['text3_content1'] = document.getElementById("osd_text3_content1").value;

    console.log(osd_settings['text3_content1'].length);
    var txt3_len = osd_settings['text3_content1'].length;
    if (txt3_len > 48) {
        alert('string count : ' + txt3_len + ' too long' + ' and Max count is 48');
        func_get_osd_cfg();
        return;
    }
    
    // osd_settings['text3_content4'] = document.getElementById("osd_text3_content4").value;
    // osd_settings['text3_content5'] = document.getElementById("osd_text3_content5").value;
    // privacy area
    osd_settings['privacy_switch'] = document.getElementById("switch_osd_privacy").value;
    osd_settings['privacy_x'] = document.getElementById("osd_privacy_x").value;
    osd_settings['privacy_y'] = document.getElementById("osd_privacy_y").value;
    osd_settings['privacy_width'] = document.getElementById("osd_privacy_width").value;
    osd_settings['privacy_hight'] = document.getElementById("osd_privacy_hight").value;
    osd_settings['privacy_color'] = document.getElementById("osd_privacy_color").value;

    var json = JSON.stringify(osd_settings);
    $.get('/cgi/set_osd_info.cgi?' + json, function (data, status) {
        func_get_osd_cfg();
        func_limit_pri_area();
    })
  });
})

function func_osd_pos_check(status) {
    // var main_resolution;
    // var sub_resolution;

    var size = {
        main_w: 0,
        main_h: 0,
        sub_w: 0,
        sub_h: 0
    }

    $.get('/cgi/get_stream_cfg.cgi', function (data, status) {
        var obj_video = JSON.parse(data, function (key, value) {
          return value;
        });
    
        size.main_w = obj_video.main_widthMax;
        size.main_h = obj_video.main_heightMax;

        size.sub_w = obj_video.sub_widthMax;
        size.sub_h = obj_video.sub_heightMax;
      })

    return size;
}

function func_limit_pri_area(status) {
  var main_resolution;
  $.get('/cgi/get_stream_cfg.cgi', function (data, status) {
    var obj_video = JSON.parse(data, function (key, value) {
      return value;
    });

    console.log(obj_video);
    main_resolution = obj_video.main_resolution
    parseInt(main_resolution);
    var resolution_w = obj_video.main_widthMax;
    var resolution_h = obj_video.main_heightMax;
    console.log(resolution_w);
    console.log(resolution_h);

    var pri_w = document.getElementById("osd_privacy_width").value;
    pri_w = parseInt(pri_w);
    var pri_h = document.getElementById("osd_privacy_hight").value;
    pri_h = parseInt(pri_h);
    var pri_x = document.getElementById("osd_privacy_x").value;
    pri_x = parseInt(pri_x);
    var pri_y = document.getElementById("osd_privacy_y").value;
    pri_y = parseInt(pri_y);
    console.log(pri_w);
    console.log(pri_h);
    console.log(pri_x);
    console.log(pri_y);
    if (pri_w + pri_x > resolution_w || pri_h + pri_y > resolution_h) {
      alert("参数越界,无效！");
    } else {
      alert("result:" + status);
    }
  })
  return;
}

function func_get_osd_cfg() {
  $.get('/cgi/get_osd_info.cgi', function (data, status) {
    console.log(status);
    var obj = JSON.parse(data, function (key, value) {
      return value;
    });

    console.log(obj);
    $("#switch_osd_global").val(obj.osd_global);
    // timestamp
    $("#switch_osd_time").val(obj.osd_time);
    $("#osd_ts_x").val(obj.osd_ts_x);
    $("#osd_ts_y").val(obj.osd_ts_y);
    // text1
    $("#switch_osd_text1").val(obj.osd_text1);
    $("#osd_text1_x").val(obj.osd_text1_x);
    $("#osd_text1_y").val(obj.osd_text1_y);
    $("#osd_text1_content").val(obj.osd_text1_content);
    // text2
    $("#switch_osd_text2").val(obj.osd_text2);
    $("#osd_text2_x").val(obj.osd_text2_x);
    $("#osd_text2_y").val(obj.osd_text2_y);
    $("#osd_text2_content").val(obj.osd_text2_content);
    // text3
    $("#switch_osd_text3").val(obj.osd_text3);
    $("#osd_text3_x").val(obj.osd_text3_x);
    $("#osd_text3_y").val(obj.osd_text3_y);
    $("#osd_text3_content1").val(obj.osd_text3_content1);
    $("#osd_text3_content2").val(obj.osd_text3_content2);
    $("#osd_text3_content3").val(obj.osd_text3_content3);
    // $("#osd_text3_content4").val(obj.osd_text3_content4);
    // $("#osd_text3_content5").val(obj.osd_text3_content5);
    // privacy area
    $("#switch_osd_privacy").val(obj.osd_privacy);
    $("#osd_privacy_x").val(obj.osd_privacy_x);
    $("#osd_privacy_y").val(obj.osd_privacy_y);
    $("#osd_privacy_width").val(obj.osd_privacy_width);
    $("#osd_privacy_hight").val(obj.osd_privacy_hight);
    $("#osd_privacy_color").val(obj.osd_privacy_color);
    $("#osd_privacy_num").val(obj.osd_privacy_num);
    document.getElementById("osd_privacy_num").innerHTML = obj.osd_privacy_num;
  })
}
