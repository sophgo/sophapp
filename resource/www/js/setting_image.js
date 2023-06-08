$(function () {
  var jmuxer = new JMuxer({
    node: 'img_sub_player',
    mode: 'video',
    flushingTime: 41,
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
  })

  $.get('/cgi/get_img_info.cgi', function (data, status) {
    var obj = JSON.parse(data, function (key, value) {
      return value;
    });

    // init brightness
    $("#brightness_slider").slider("setValue", obj.brightness);
    $("#brightness_slider_label").text(obj.brightness);

    // init sharpness
    $("#sharpness_slider").slider("setValue", obj.sharpness);
    $("#sharpness_slider_label").text(obj.sharpness);

    // init chroma
    $("#chroma_slider").slider("setValue", obj.chroma);
    $("#chroma_slider_label").text(obj.chroma);

    // init contrast
    $("#contrast_slider").slider("setValue", obj.contrast);
    $("#contrast_slider_label").text(obj.contrast);

    // init saturation
    $("#saturation_slider").slider("setValue", obj.saturation);
    $("#saturation_slider_label").text(obj.saturation);

    // init noise2d
    $("#noise2d_slider").slider("setValue", obj.noise2d);
    $("#noise2d_slider_label").text(obj.noise2d);

    // init noise3d
    $("#noise3d_slider").slider("setValue", obj.noise3d);
    $("#noise3d_slider_label").text(obj.noise3d);

    // init compensation switch
    console.log(obj.compensationEnabled);
    document.querySelector('#switch_compensation').checked = obj.compensationEnabled;

    // init compensation
    $("#compensation_slider").slider("setValue", obj.compensation);
    $("#compensation_slider_label").text(obj.compensation);
    if (obj.compensationEnabled == 1) {
      $("#compensation_slider").slider("enable");
    }

    // init suppression switch
    document.querySelector('#switch_suppression').checked = obj.suppressionEnabled;

    // init suppression
    $("#suppression_slider").slider("setValue", obj.suppression);
    $("#suppression_slider_label").text(obj.suppression);
    if (obj.suppressionEnabled == 1) {
      $("#suppression_slider").slider("enable");
    }

    // init white balance enable
    document.getElementById("switch_whiteBalance")[obj.whiteBalance].selected = true;

    // init red gain
    $("#red_gain_slider").slider("setValue", obj.red_gain);
    $("#red_gain_slider_label").text(obj.red_gain);
    // init blue gain
    $("#blue_gain_slider").slider("setValue", obj.blue_gain);
    $("#blue_gain_slider_label").text(obj.blue_gain);

    if (obj.whiteBalance == 1) {
      $("#red_gain_slider").slider("enable");
      $("#blue_gain_slider").slider("enable");
    }

    // init defog switch
    document.querySelector('#switch_defog').checked = obj.defogEnabled;
    // init defog
    $("#defog_slider").slider("setValue", obj.defog);
    $("#defog_slider_label").text(obj.defog);
    if (obj.defogEnabled == 1) {
      $("#defog_slider").slider("enable");
    }

    // init shutter switch
    document.querySelector('#switch_shutter').checked = obj.shutterEnabled;
    // init shutter
    $("#shutter_slider").slider("setValue", obj.shutter);
    $("#shutter_slider_label").text(obj.shutter);
    if (obj.shutterEnabled == 1) {
      $("#shutter_slider").slider("enable");
    }

    // init distortion switch
    document.querySelector('#switch_distortion').checked = obj.distortionEnabled;
    // init distortion
    $("#distortion_slider").slider("setValue", obj.distortion);
    $("#distortion_slider_label").text(obj.distortion);
    if (obj.distortionEnabled == 1) {
      $("#distortion_slider").slider("enable");
    }

    // init frequency
    document.getElementById("switch_videoFormat")[obj.frequency].selected = true;
    // init antiflash
    document.getElementById("switch_antiFlash")[obj.antiflashEnabled].selected = true;
    // init hflip
    document.getElementById("switch_horizontalFlip")[obj.hflipEnabled].selected = true;
    // init vflip
    document.getElementById("switch_verticallyFlip")[obj.vflipEnabled].selected = true;
    // init wdr
    document.getElementById("switch_WDR")[obj.wdrEnabled].selected = true;
    // init ir cut
    document.getElementById("switch_IRcut")[obj.irCutEnabled].selected = true;
    // init ir cut manual
    document.getElementById("switch_IRcutManual")[obj.irCutEnabledManual].selected = true;
    // init keep color
    document.getElementById("switch_keepColor")[obj.keepColorEnabled].selected = true;
    // init dis
    document.getElementById("switch_dis")[obj.disEnabled].selected = true;
    // init ds
    document.getElementById("switch_ds")[obj.dsEnabled].selected = true;
  })

  $("#switch_videoFormat").change(function () {
    $.get('/cgi/set_img_info.cgi?frequency=' + this.value, function (data, status) {
    })
  })

  $("#switch_horizontalFlip").change(function () {
    $.get('/cgi/set_img_info.cgi?hflip=' + this.value, function (data, status) {
    })
  })

  $("#brightness_slider").slider();
  $("#brightness_slider").on("change", function (e) {
    // console.log("slider change" + e.value.oldValue + "--" + e.value.newValue);
    $("#brightness_slider_label").text(e.value.newValue);
  });
  $("#brightness_slider").on("slideStop", function (e) {
    $.get('/cgi/set_img_info.cgi?brightness=' + e.value, function (data, status) {
    });
  });

  // sharpness
  $("#sharpness_slider").slider();
  $("#sharpness_slider").on("change", function (e) {
    $("#sharpness_slider_label").text(e.value.newValue);
  });
  $("#sharpness_slider").on("slideStop", function (e) {
    $.get('/cgi/set_img_info.cgi?sharpness=' + e.value, function (data, status) {
    });
  });

  // chroma
  $("#chroma_slider").slider();
  $("#chroma_slider").on("change", function (e) {
    $("#chroma_slider_label").text(e.value.newValue);
  });
  $("#chroma_slider").on("slideStop", function (e) {
    func_cgi_req('/cgi/set_img_info.cgi?chroma=' + e.value);
  });

  // contrast
  $("#contrast_slider").slider();
  $("#contrast_slider").on("change", function (e) {
    $("#contrast_slider_label").text(e.value.newValue);
  });
  $("#contrast_slider").on("slideStop", function (e) {
    func_cgi_req('/cgi/set_img_info.cgi?contrast=' + e.value);
  });

  // saturation
  $("#saturation_slider").slider();
  $("#saturation_slider").on("change", function (e) {
    $("#saturation_slider_label").text(e.value.newValue);
  });
  $("#saturation_slider").on("slideStop", function (e) {
    func_cgi_req('/cgi/set_img_info.cgi?saturation=' + e.value);
  });

  // 2D noise reduction
  $("#noise2d_slider").slider();
  $("#noise2d_slider").on("change", function (e) {
    $("#noise2d_slider_label").text(e.value.newValue);
  });
  $("#noise2d_slider").on("slideStop", function (e) {
    func_cgi_req('/cgi/set_img_info.cgi?noise2d=' + e.value);
  });

  // 3D noise reduction
  $("#noise3d_slider").slider();
  $("#noise3d_slider").on("change", function (e) {
    $("#noise3d_slider_label").text(e.value.newValue);
  });
  $("#noise3d_slider").on("slideStop", function (e) {
    func_cgi_req('/cgi/set_img_info.cgi?noise3d=' + e.value);
  });

  // compensation
  $("#switch_compensation").click(function () {
    var value = 0;
    if (this.checked) {
      $("#compensation_slider").slider("enable");
      value = 1;
    } else {
      $("#compensation_slider").slider("disable");
    }
    func_cgi_req('/cgi/set_img_info.cgi?compensation_enable=' + value);
  });

  $("#compensation_slider").slider();
  $("#compensation_slider").slider("disable");
  $("#compensation_slider").on("change", function (e) {
    $("#compensation_slider_label").text(e.value.newValue);
  });
  $("#compensation_slider").on("slideStop", function (e) {
    func_cgi_req('/cgi/set_img_info.cgi?compensation=' + e.value);
  });

  // suppression
  $("#switch_suppression").click(function () {
    var value = 0;
    if (this.checked) {
      $("#suppression_slider").slider("enable");
      value = 1;
    } else {
      $("#suppression_slider").slider("disable");
    }
    func_cgi_req('/cgi/set_img_info.cgi?suppression_enable=' + value);
  });

  $("#suppression_slider").slider();
  $("#suppression_slider").slider("disable");
  $("#suppression_slider").on("change", function (e) {
    $("#suppression_slider_label").text(e.value.newValue);
  });
  $("#suppression_slider").on("slideStop", function (e) {
    func_cgi_req('/cgi/set_img_info.cgi?suppression=' + e.value);
  });

  $("#smart_btn_delete").click(function () {
    $.get('/cgi/clear_smart_rgn.cgi', function (data, status) {
      smart_range_empty();
    })
  })

  $("#smart_btn_save_all").click(function () {
    console.log("smart_range_save_all");
    console.log(pointArrLine);
    console.log(pointArrRange);

    var smart_range = {};
    smart_range['line_count'] = pointArrLine.length;
    smart_range['lines'] = pointArrLine;
    smart_range['rgn_count'] = pointArrRange.length;
    smart_range['rgns'] = pointArrRange;

    if ( pointArrRange == 0 || pointArrRange.length == 0) {
      smart_range_empty();
      alert("区域为空！");
    }

    var json = JSON.stringify(smart_range);
    $.get('/cgi/set_smart_rgn.cgi?' + json, function (data, status) {
      smart_range_empty();
    });
  })
})

function func_switch_whiteBalance(value) {
  if (value == 0) {
    $("#red_gain_slider").slider("disable"); 
    $("#blue_gain_slider").slider("disable");
  } else {
    $("#red_gain_slider").slider("enable");
    $("#blue_gain_slider").slider("enable");
  }
  func_cgi_req('/cgi/set_img_info.cgi?whiteBalance=' + value);
}

// blue gain
$("#blue_gain_slider").slider();
$("#blue_gain_slider").slider("disable");
$("#blue_gain_slider").on("slide", function (slideEvt) {
  $("#blue_gain_slider_label").text(slideEvt.value);
});
$("#blue_gain_slider").on("slideStop", function (e) {
  func_cgi_req('/cgi/set_img_info.cgi?blue_gain=' + e.value);
});

// red gain
$("#red_gain_slider").slider();
$("#red_gain_slider").slider("disable");
$("#red_gain_slider").on("slide", function (slideEvt) {
  $("#red_gain_slider_label").text(slideEvt.value);
});
$("#red_gain_slider").on("slideStop", function (e) {
  func_cgi_req('/cgi/set_img_info.cgi?red_gain=' + e.value);
});

// defog
$("#switch_defog").click(function () {
  var value = 0;
  if (this.checked) {
    $("#defog_slider").slider("enable");
    value = 1;
  } else {
    $("#defog_slider").slider("disable");
  }
  func_cgi_req('/cgi/set_img_info.cgi?defog_enable=' + value);
});

$("#defog_slider").slider();
$("#defog_slider").slider("disable");
$("#defog_slider").on("change", function (e) {
  $("#defog_slider_label").text(e.value.newValue);
});
$("#defog_slider").on("slideStop", function (e) {
  func_cgi_req('/cgi/set_img_info.cgi?defog=' + e.value);
});

// shutter
$("#switch_shutter").click(function () {
  var value = 0;
  if (this.checked) {
    $("#shutter_slider").slider("enable");
    value = 1;
  } else {
    $("#shutter_slider").slider("disable");
  }
  func_cgi_req('/cgi/set_img_info.cgi?shutter_enable=' + value);
});

$("#shutter_slider").slider();
$("#shutter_slider").slider("disable");
$("#shutter_slider").on("change", function (e) {
  $("#shutter_slider_label").text(e.value.newValue);
});
$("#shutter_slider").on("slideStop", function (e) {
  func_cgi_req('/cgi/set_img_info.cgi?shutter=' + e.value);
});

// distortion
$("#switch_distortion").click(function () {
  var value = 0;
  if (this.checked) {
    $("#distortion_slider").slider("enable");
    value = 1;
  } else {
    $("#distortion_slider").slider("disable");
  }
  func_cgi_req('/cgi/set_img_info.cgi?distortion_enable=' + value);
});

$("#distortion_slider").slider();
$("#distortion_slider").slider("disable");
$("#distortion_slider").on("change", function (e) {
  $("#distortion_slider_label").text(e.value.newValue);
});
$("#distortion_slider").on("slideStop", function (e) {
  func_cgi_req('/cgi/set_img_info.cgi?distortion=' + e.value);
});

// setting send to board
function func_cgi_req(cgi_cmd) {
  console.log(cgi_cmd);
  var http_req = new XMLHttpRequest();
  http_req.open('GET', cgi_cmd, true);
  http_req.send(null);
  http_req.onload = function () {
    console.log(http_req.responseText);
    // img_info = http_req.responseText;
    // return http_req.responseText;
  }
}

function func_switch_antiflash(value) {
  console.log("switch_antiFlash index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?antiflash=' + value);
}

function func_switch_vflip(value) {
  console.log("switch_verticallyFlip index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?vflip=' + value);
}

function func_switch_wdr(value) {
  console.log("switch_WDR index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?wdr=' + value);
}

function func_switch_ircut(value) {
  console.log("switch_IRcut index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?ircut=' + value);
  func_cgi_req('/cgi/set_img_info.cgi?manual_ir=' + 0);
  var manual;
  manual = document.getElementById("switch_IRcutManual").value;
  console.log("manual " + manual);
  if (manual == true) {
    $("#switch_IRcutManual").val(0);
    $("#switch_IRcut").val(1);
  }
}

function func_switch_ircutManual(value) {
  console.log("switch_IRcutManual index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?manual_ir=' + value);
  func_cgi_req('/cgi/set_img_info.cgi?ircut=' + 0);
  var ircut;
  ircut = document.getElementById("switch_IRcut").value;
  console.log("ircut " + ircut);
  if (ircut == true) {
    $("#switch_IRcut").val(0);
    $("#switch_IRcutManual").val(1);
  }
}

function func_switch_ircutManualStatus(value) {
  console.log("switch_IRcutManualStatus index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?stateIR=' + value);
}

function func_switch_keepcolor(value) {
  console.log("switch_keepColor index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?keepcolor=' + value);
}

function func_switch_dis(value) {
  console.log("switch_dis index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?dis=' + value);
}

function func_switch_ds(value) {
  console.log("switch_ds index " + value);
  func_cgi_req('/cgi/set_img_info.cgi?ds=' + value);
}


var can = document.getElementById("canvas");
var ctx = can.getContext('2d');
var canSave = document.getElementById("canvasSave");
var ctxSave = canSave.getContext('2d');

var pointX, pointY;
var pointArr = [];//存放坐标的数组
var pointArrSave = [];
var pointArrLine = [];
var pointArrRange = [];

ctx.strokeStyle = 'rgba(255, 0, 0, 1)';//线条颜色
ctx.lineWidth = 2;//线条粗细
ctxSave.strokeStyle = 'rgba(255, 0, 0, 1)';//线条颜色
ctxSave.lineWidth = 2;//线条粗细

var oIndex = -1;//判断鼠标是否移动到起始点处，-1为否，1为是

/*点击画点*/
$(can).click(function (e) {
  console.log("click canvas");
  if (e.offsetX || e.layerX) {
    pointX = e.offsetX == undefined ? e.layerX : e.offsetX;
    pointY = e.offsetY == undefined ? e.layerY : e.offsetY;
    var piX, piY;
    if (oIndex > 0 && pointArr.length > 0) {
      piX = pointArr[0].x;
      piY = pointArr[0].y;
      //画点
      makearc(ctx, piX, piY, GetRandomNum(2, 2), 0, 180, 'rgba(102,168,255,1)');
      pointArr.push({ x: piX, y: piY });
      pointArrSave.push({ x: piX, y: piY });
      canvasSave(pointArr);//保存点线同步到另一个canvas
      saveCanvas();//生成画布
    } else {
      piX = pointX;
      piY = pointY;
      makearc(ctx, piX, piY, GetRandomNum(2, 2), 0, 180, 'rgba(102,168,255,1)');
      pointArr.push({ x: piX, y: piY });
      pointArrSave.push({ x: piX, y: piY });
      canvasSave(pointArr);//保存点线同步到另一个canvas
    }
  }
});

$(can).mouseleave(function (e) {
  console.log("mouse leave");
  ctx.clearRect(0, 0, 640, 360);//清空画布
});

$(can).mousemove(function (e) {
  console.log("mousemove");
  if (e.offsetX || e.layerX) {
    pointX = e.offsetX == undefined ? e.layerX : e.offsetX;
    pointY = e.offsetY == undefined ? e.layerY : e.offsetY;
    var piX, piY;
    /*清空画布*/
    ctx.clearRect(0, 0, can.width, can.height);
    /*鼠标下跟随的圆点*/
    makearc(ctx, pointX, pointY, GetRandomNum(4, 4), 0, 180, 'rgba(102,168,255,1)');

    if (pointArr.length > 0) {
      if ((pointX > pointArr[0].x - 15 && pointX < pointArr[0].x + 15) && (pointY > pointArr[0].y - 15 && pointY < pointArr[0].y + 15)) {
        if (pointArr.length > 1) {
          piX = pointArr[0].x;
          piY = pointArr[0].y;
          ctx.clearRect(0, 0, can.width, can.height);
          makearc(ctx, piX, piY, GetRandomNum(4, 4), 0, 180, 'rgba(102,168,255,1)');
          oIndex = 1;
        }
      } else {
        piX = pointX;
        piY = pointY;
        oIndex = -1;
      }
      /*开始绘制*/
      ctx.beginPath();
      ctx.moveTo(pointArr[0].x, pointArr[0].y);
      if (pointArr.length > 1) {
        for (var i = 1; i < pointArr.length; i++) {
          ctx.lineTo(pointArr[i].x, pointArr[i].y);
        }
      }
      ctx.lineTo(piX, piY);
      // ctx.fillStyle = 'rgba(200,195,255,1)';//填充颜色
      // ctx.fill();//填充
      ctx.stroke();//绘制
    }
  }
});

// 存储已生成的点线
function canvasSave(pointArr) {
  console.log("canvas save");
  ctxSave.clearRect(0, 0, ctxSave.width, ctxSave.height);
  ctxSave.beginPath();
  if (pointArr.length > 1) {
    ctxSave.moveTo(pointArr[0].x, pointArr[0].y);
    for (var i = 1; i < pointArr.length; i++) {
      ctxSave.lineTo(pointArr[i].x, pointArr[i].y);
      // ctxSave.fillStyle = 'rgba(200,195,255,1)';//填充颜色
      // ctxSave.fill();
      ctxSave.stroke();//绘制
    }
    ctxSave.closePath();
  }
  if (pointArr.length > 20) {
    alert("区域点超过20,请先清除区域后再重新绘制");
  }
}

/*生成画布 结束绘画*/
function saveCanvas() {
  console.log("save canvas");
  ctx.clearRect(0, 0, can.width, can.height);
  ctxSave.closePath();//结束路径状态，结束当前路径，如果是一个未封闭的图形，会自动将首尾相连封闭起来
  // ctxSave.fillStyle = 'rgba(220,220,220,128)';//填充颜色
  // ctxSave.fill();//填充
  ctxSave.stroke();//绘制
  pointArr = [];
}

function changeSaveColor() {
  console.log("change save coloe");
  if (pointArrSave.length != 0) {
    ctx.clearRect(0, 0, can.width, can.height);
    ctxSave.strokeStyle = 'rgba(0, 128, 0, 1)';//线条颜色
    ctxSave.closePath();
    // ctxSave.fillStyle = 'rgba(211,211,211,128)';//填充颜色
    // ctxSave.fill();//填充
    ctxSave.stroke();//绘制
  }
  pointArr = [];
  ctxSave.strokeStyle = 'rgba(255, 0, 0, 1)';//线条颜色
}

/*清空选区*/
$('#deleteCanvas').click(function () {
  console.log("delete canvas");
  ctx.clearRect(0, 0, can.width, can.height);
  ctxSave.clearRect(0, 0, canSave.width, canSave.height);
  pointArr = [];
  pointArrSave = [];
  pointArrAll = [];
});

/*验证canvas画布是否为空函数*/
function isCanvasBlank(canvas) {
  console.log("isCanvasBlank");
  var blank = document.createElement('canvas');//创建一个空canvas对象
  blank.width = canvas.width;
  blank.height = canvas.height;
  return canvas.toDataURL() == blank.toDataURL();//为空 返回true
}

/*canvas生成圆点*/
function GetRandomNum(Min, Max) {
  var Range = Max - Min;
  var Rand = Math.random();
  return (Min + Math.round(Rand * Range));
}

function makearc(ctx, x, y, r, s, e, color) {
  ctx.clearRect(0, 0, 199, 202);//清空画布
  ctx.beginPath();
  ctx.fillStyle = color;
  ctx.arc(x, y, r, s, e);
  ctx.fill();
}

function smart_range_save() {
  oIndex = -1;
  // canvasSave(pointArr);
  // saveCanvas();
  changeSaveColor();
  console.log("smart_range_save()" + pointArrSave.length);
  if (pointArrSave.length == 2) {
    pointArrLine.push(pointArrSave);
  } else if (pointArrSave.length != 0) {
    pointArrRange.push(pointArrSave);
  }
  pointArrSave = [];
}


function smart_range_empty() {
  console.log("smart_range_empty");
  console.log("delete canvas");
  ctx.clearRect(0, 0, can.width, can.height);
  ctxSave.clearRect(0, 0, canSave.width, canSave.height);
  pointArr = [];
  pointArrSave = [];
  pointArrLine = [];
  pointArrRange = [];
}
