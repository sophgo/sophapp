$(function () {
  document.getElementById("crop_canvas").style.display = "none";

  $.get('/cgi/get_cur_chn.cgi', function (data, status) {
    console.log(status);
    var obj = JSON.parse(data, function (key, value) {
      return value;
    });

    $("#switch_stream").val(obj.current_chn);
  });

  $("#capture").click(function () {
    $.get('/cgi/takePhoto', function (data, status) {
    });
  });

  $("#switch_stream").change(function () {
    $.get('/cgi/switch_stream.cgi?chn=' + this.value, function (data, status) {
      window.location.reload();
    });
  });

  var wsUrl = null;
  var lockReconnect = false;
  var webScoket = null;
  var tt = null;
  var hiddened = false;
  var jmuxer = new JMuxer({
    node: 'player',
    mode: 'video',
    flushingTime: 40,
    fps: 25,
    clearBuffer: true,
    debug: false,
  });

  document.addEventListener('visibilitychange', function () {
    if (document.hidden) {
      hiddened = true;
    } else {
      hiddened = false;
    }
  });

  var timestamp = 0;
  var frameCount = 0;
  var canvas = document.getElementById("canvas_overlay");
  var ctx = canvas.getContext("2d");

  ctx.fillStyle = "#FF0000";
  ctx.font = "30px Arial";
  ctx.textBaseline = "top";
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle'

  var needReset = 0;
  $.get('/cgi/get_ws_addr.cgi', function (data, status) {
    console.log("enter connectWS, addr: " + data);

    if (data == "ws://0.0.0.0:8000"){
      alert("只支持单路播放");
      return
    }

    wsUrl = data;
    webScoket = new WebSocket(wsUrl);
    webScoket.binaryType = 'arraybuffer';
    webSocketInit(wsUrl);
  })

  function webSocketInit(data) {
    webScoket.onopen = function (evt) {
      console.log('Connection open ...');
      heartCheck.start();
    };

    webScoket.onerror = function (evt) {
      console.log('WS error ' + evt.message);
      reconnect(data);
    }

    webScoket.onclose = function () {
      console.log("WS closed");
      reconnect(data);
    }

    webScoket.onmessage = function (evt) {
      var buffer = new Uint8Array(evt.data);
      var type = buffer[0];
      heartCheck.start();

      if (hiddened == true) {
        needReset = 1;
        return;
      }

      if (needReset == 1) {
        needReset = 0;
        jmuxer.reset();
      }

      if (type == 0) {
        var myDate = new Date().getTime();

        ctx.fillStyle = "#FF0000";
        ctx.font = "30px Arial";
        frameCount++;
        if ((myDate - timestamp) > 1000) {
          timestamp = myDate;
          ctx.clearRect(0, 0, 1280, 50);
          ctx.fillText("视频帧率:" + frameCount, 1100, 30);
          frameCount = 0;
        }

        jmuxer.feed({
          video: buffer
        });
      } else if (type == 1) {
        console.log("got a crop data");
        func_show_crop4(buffer);
      } else if (type == 2) {
        ctx.clearRect(0, 50, 1280, 100);
        ctx.fillText(String.fromCharCode.apply(null, buffer.slice(1)), 1150, 80);
      } else if (type == 3) {
        ctx.clearRect(0, 80, 1280, 300);
        // ctx.fillStyle = "#66CCFF";
        // ctx.font = "30px Arial";
        // if (String.fromCharCode.apply(null, buffer.slice(1,3)) >= "02") {
        //   ctx.clearRect(0, 80, 1280, 110);
        //   ctx.fillStyle = "#ff6600";
        //   ctx.font = "30px Arial";
        //   ctx.fillText("区域人数:" +String.fromCharCode.apply(null, buffer.slice(1,3)), 1100, 110);
        // } else {
        //   ctx.fillText("区域人数:" +String.fromCharCode.apply(null, buffer.slice(1,3)), 1100, 110);
        // }
        ctx.fillStyle = "#66CCFF";
        ctx.font = "30px Arial";
        // ctx.fillText("人流量:" +String.fromCharCode.apply(null, buffer.slice(3)), 1100, 150);
        ctx.fillText("PD帧率:" +String.fromCharCode.apply(null, buffer.slice(1,2)), 1100, 100);
        ctx.fillText("MD帧率:" +String.fromCharCode.apply(null, buffer.slice(3,4)), 1100, 150);
        ctx.fillText("PD入侵:" +String.fromCharCode.apply(null, buffer.slice(5,6)), 1100, 200);
      }
    }
  }

  function reconnect(url) {
    if(lockReconnect) {
      return;
    }
    lockReconnect = true;
    tt && clearTimeout(tt);
    tt = setTimeout(function () {
      createWebSocket();
      $.get('/cgi/get_cur_chn.cgi', function (data, status) {
        console.log(status);
        var obj = JSON.parse(data, function (key, value) {
          return value;
        });

        $("#switch_stream").val(obj.current_chn);
      });
      lockReconnect = false;
    }, 4000)
  }

  function createWebSocket() {
    try {
      webScoket = new WebSocket(wsUrl);
      webScoket.binaryType = 'arraybuffer';

      webSocketInit(wsUrl);
    } catch(e) {
      console.log("catch");
      reconnect(wsUrl);
    }
  }

  var heartCheck = {
    timeout: 3000,
    timeoutObj: null,
    serverTimeoutObj: null,
    start: function() {
      this.timeoutObj && clearTimeout(this.timeoutObj);
      this.serverTimeoutObj && clearTimeout(this.serverTimeoutObj);
      this.timeoutObj = setTimeout(function() {
        console.log("send message, testing hearCheck");
        this.serverTimeoutObj = setTimeout(function() {
          console.log("no heat beat...");
          webScoket.close();
        }, this.timeout)

      }, this.timeout)
    }
  }

  function func_show_crop2(cropData) {
    console.log(cropData);
    var buffer = new Uint8Array(cropData);

    let width, height;
    let canvas = document.getElementById("crop_canvas");
    let ctx = canvas.getContext("2d");
    var imgData = ctx.createImageData(width, height);
    for (var i = 0, j = 0; i < imgData.data.length; i += 4, j += 3) {

      imgData.data[i + 0] = buffer[j + 0];
      imgData.data[i + 1] = buffer[j + 1];
      imgData.data[i + 2] = buffer[j + 2];
      imgData.data[i + 3] = 255;
    }
    ctx.putImageData(imgData, 0, 0);

    var img = g_arr[g_img_index];
    img.src = canvas.toDataURL();
    g_img_index++;

    if (g_img_index >= 6) {
      g_img_index = 0;
    }
  }

  function func_show_crop3(aiList, cropData) {
    console.log(cropData);
    var buffer = new Uint8Array(cropData);

    let width = 1920, height = 1080;
    let canvas = document.getElementById("crop_canvas");
    let ctx = canvas.getContext("2d");
    var imgData = ctx.createImageData(width, height);
    for (var i = 0, j = 0; i < imgData.data.length; i += 4, j += 3) {

      imgData.data[i + 0] = buffer[j + 0];
      imgData.data[i + 1] = buffer[j + 1];
      imgData.data[i + 2] = buffer[j + 2];
      imgData.data[i + 3] = 255;
    }
    ctx.putImageData(imgData, 0, 0);

    console.log("ailist size:" + ailist.length);
    for (var i = 0; i < ailist.length; i++) {
      var oneai = ailist[i];
      console.log(oneai.getRect().getX1() + ' ' + oneai.getRect().getY1() + ' ' + oneai.getRect().getX2() + ' ' + oneai.getRect().getY2());
      var x = oneai.getRect().getX1();
      var y = oneai.getRect().getY1();
      var w = oneai.getRect().getX2() - oneai.getRect().getX1();
      var h = oneai.getRect().getY2() - oneai.getRect().getY1();
      if ((w > 10) && (h > 10)) {
        captureImage3(canvas, x, y, w, h);
      }
    }

  }


  function func_show_crop4(buffer) {
    console.log("func_show_crop4 ", buffer);

    let width = 192, height = 192;
    let canvas = document.getElementById("crop_canvas");
    let ctx = canvas.getContext("2d");
    var imgData = ctx.createImageData(width, height);

    var type = buffer[0];
    var ai_count = buffer[1];

    console.log("type and count ", type, " ", ai_count);

    for (var i = 0, j = 2 + ai_count * 8; i < imgData.data.length; i += 4, j += 3) {

      imgData.data[i + 0] = buffer[j + 0];
      imgData.data[i + 1] = buffer[j + 1];
      imgData.data[i + 2] = buffer[j + 2];
      imgData.data[i + 3] = 255;
    }
    ctx.putImageData(imgData, 0, 0);

    var offset = 2;
    for (var i = 0; i < ai_count; i++) {

      var x = buffer[offset++] + (buffer[offset++] << 8);
      var y = buffer[offset++] + (buffer[offset++] << 8);
      var x2 = buffer[offset++] + (buffer[offset++] << 8);
      var y2 = buffer[offset++] + (buffer[offset++] << 8);

      var w = x2 - x;
      var h = y2 - y;
      console.log(x, y, x2, y2);
      console.log(x, y, w, h);

      // if ((w > 10) && (h > 10)) {
      //   captureImage3(canvas, x, y, w, h);
      // }

      let arr = document.getElementsByClassName('catch_img')
      // var img = document.getElementById("img1");
      var img = arr[g_img_index];
      img.src = canvas.toDataURL();
      console.log("this.index ", g_img_index);
      g_img_index++;

      if (g_img_index >= 6) {
        g_img_index = 0;
      }
    }
  }
  $.get('/cgi/get_hemlet_flag.cgi', function (data, status) {
    console.log(status);
    var hobj = JSON.parse(data, function (key, hvalue) {
      return hvalue;
    });
    console.log(hobj.hemlet_flag);
    if (hobj.hemlet_flag == 1) {
      $("#helmet").attr('checked',true).siblings().attr('checked',false);
    } else {
      $("#helmet").attr('checked',false).siblings().attr('checked',true);
    }
    
  });
  $.get('/cgi/get_mask_flag.cgi', function (data, status) {
    console.log(status);
    var mobj = JSON.parse(data, function (key, mvalue) {
      return mvalue;
    });
    console.log(mobj.mask_flag);
    if (mobj.mask_flag == 1) {
      $("#mask").attr('checked',true).siblings().attr('checked',false);
    } else {
      $("#mask").attr('checked',false).siblings().attr('checked',true);
    }
  });

  $("#helmet").click(function () {
    if ($(this).is(":checked")== true) {
      $(this).attr('checked',true).siblings().attr('checked',false);
      //选中触发事件
      var helmet_flag = 1;
      $.get('/cgi/set_hemlet_flag.cgi?flag=' + helmet_flag, function (data, status) {
      });
      var mask_flag = 0;
      $.get('/cgi/set_mask_flag?flag=' + mask_flag, function (data, status) {
      });
    }
  });
  $("#mask").click(function () {
    if ($(this).is(":checked")== true) {
      $(this).attr('checked',true).siblings().attr('checked',false);
      var mask_flag = 1;
      $.get('/cgi/set_mask_flag?flag=' + mask_flag, function (data, status) {
      });
      var helmet_flag = 0;
      $.get('/cgi/set_hemlet_flag.cgi?flag=' + helmet_flag, function (data, status) {
      });
    }
  });
})

function captureImage() {
  console.log("capturing111");

  var tmp_video = document.getElementById("player");
  var tmp_canvas = document.getElementById("canvas_overlay");
  var ctx = tmp_canvas.getContext('2d');

  var x1 = document.getElementById("cropX1").value;
  var y1 = document.getElementById("cropY1").value;
  var x2 = document.getElementById("cropX2").value;
  var y2 = document.getElementById("cropY2").value;
  console.log("x1 ", x1, " y1 ", y1, " x2 ", x2, " y2 ", y2, " x2-x1 ", x2 - x1, " y2-y1 ", y2 - y1);

  // ctx.drawImage(tmp_video, 0, 0, tmp_canvas.width, tmp_canvas.height);
  ctx.drawImage(tmp_video, x2, y1, x1 - x2, y2 - y1, 0, 0, 1280, 720);
  // ctx.drawImage(tmp_video, x, y, x+100, y+100);

  // var img = document.getElementById("img1");
  var img = g_arr[g_img_index];
  img.src = tmp_canvas.toDataURL();
  console.log("this.index ", g_img_index);
  g_img_index++;

  if (g_img_index >= 6) {
    g_img_index = 0;
  }

  ctx.clearRect(0, 0, tmp_canvas.width, tmp_canvas.height);
}

function captureImage2(x, y, width, height) {
  console.log("capturing2");

  var tmp_video = document.getElementById("player");
  var tmp_canvas = document.getElementById("canvas_overlay");
  tmp_canvas.setAttribute("width", width)
  tmp_canvas.setAttribute("height", height)
  var ctx = tmp_canvas.getContext('2d');

  // console.log("x ", x, " y ", y, "w ", width, "h ", height);

  // ctx.drawImage(tmp_video, x, y, width, height, 0, 0, 1280, 720);
  ctx.drawImage(tmp_video, x, y, width, height, 0, 0, width, height);

  let arr = document.getElementsByClassName('catch_img')
  // var img = document.getElementById("img1");
  var img = arr[g_img_index];
  img.src = tmp_canvas.toDataURL();
  console.log("this.index ", g_img_index);
  g_img_index++;

  if (g_img_index >= 6) {
    g_img_index = 0;
  }

  ctx.clearRect(0, 0, tmp_canvas.width, tmp_canvas.height);
}

var g_img_index = 0;

function captureImage3(canvas, x, y, width, height) {
  console.log("capturing2");

  var tmp_video = document.getElementById("player");
  var tmp_canvas = document.getElementById("canvas_overlay");
  tmp_canvas.setAttribute("width", width)
  tmp_canvas.setAttribute("height", height)
  var ctx = tmp_canvas.getContext('2d');

  // console.log("x ", x, " y ", y, "w ", width, "h ", height);

  // ctx.drawImage(tmp_video, x, y, width, height, 0, 0, 1280, 720);
  ctx.drawImage(canvas, x, y, width, height, 0, 0, width, height);

  let arr = document.getElementsByClassName('catch_img')
  // var img = document.getElementById("img1");
  var img = arr[g_img_index];
  img.src = tmp_canvas.toDataURL();
  console.log("this.index ", g_img_index);
  g_img_index++;

  if (g_img_index >= 6) {
    g_img_index = 0;
  }

  ctx.clearRect(0, 0, tmp_canvas.width, tmp_canvas.height);
}
