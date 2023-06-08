$(function () {   
    $("#play").click(function () {
        decode_seq($("#decode_type").val());
    });
})

var webglPlayer, canvas;
var LOG_LEVEL_JS = 0;
var LOG_LEVEL_WASM = 1;
var LOG_LEVEL_FFMPEG = 2;
var DECODER_H264 = 0;
var DECODER_H265 = 1;
var pts = 0;
var chunk_size = 150*1024;
var cacheBuffer = null 

function decode_seq(decoder_type) {
    var videoSize = 0;
    var videoCallback = Module.addFunction(function (addr_y, addr_u, addr_v, stride_y, stride_u, stride_v, width, height, pts) {
        //console.log("[%d]In video callback, size = %d * %d, pts = %d", ++videoSize, width, height, pts)
        let size = width * height;
        let y_data = HEAPU8.subarray(addr_y, addr_y + size)
        //y_data = new Uint8Array(y_data)

        let u_data = HEAPU8.subarray(addr_u, addr_u + size/2)
        //u_data = new Uint8Array(u_data)

        let v_data = HEAPU8.subarray(addr_v, addr_v + size/2)
        //v_data = new Uint8Array(v_data)

        displayVideoFrame(y_data, u_data, v_data, width, height);
    },"viiiiiiiii");

    var ret = Module._openDecoder(decoder_type, videoCallback, LOG_LEVEL_WASM)
    if(ret == 0) {
        console.log("openDecoder success");
    } else {
        console.error("openDecoder failed with error", ret);
        return;
    }

    cacheBuffer = Module._malloc(chunk_size);

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
            var typedArray = new Uint8Array(evt.data);
            var type = typedArray[0];
            var size = typedArray.length
            if (size > chunk_size){
                console.log("frame size out of bound");
                return;
            }

            if (type == 0) {
                Module.HEAPU8.set(typedArray, cacheBuffer);
                Module._decodeData(cacheBuffer, size, pts++)
            }
        }
    });
}

function displayVideoFrame(y_data, u_data, v_data, width, height) {
    if(!webglPlayer) {
        const canvasId = "playCanvas";
        canvas = document.getElementById(canvasId);
        webglPlayer = new WebGLPlayer(canvas, {
            preserveDrawingBuffer: false
        });
    }
    webglPlayer.renderFrame(y_data, u_data, v_data, width, height);
}
