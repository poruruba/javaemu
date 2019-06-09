'use strict';

var g_pointer = null;
var g_buffer = null;

const BUFFER_SIZE = 1024;

var vue_options = {
    el: "#top",
    data: {
        progress_title: '',

        jclasses_file: null,
        jclass_list: [],
        class_name: null,
        input_arg: "",
        input_params: "",
        output_return: "",
        output_params: ""        
    },
    computed: {
    },
    methods: {
        jclasses_read: function(){
            var reader = new FileReader();
            var parent = this;
            reader.onload = function(theFile){
                var bin = jar2bin(reader.result);
                parent.jclass_list = bin.names;
                if( g_pointer != null ){
                    Module._free(g_pointer);
                    g_pointer = null;
                }
                g_pointer = Module._malloc(bin.array.length);
                Module.HEAPU8.set(bin.array, g_pointer);
                var ret = Module.ccall('setRomImage', "number", ["number", "number"], [g_pointer, bin.array.length]);
                console.log("setRomImage ret=" + ret);        
            };
            if( jclasses_file.files.length == 0 ){
                var ret = Module.ccall('setRomImage', "number", ["number", "number"], [0, 0]);
                console.log("setRomImage ret=" + ret);        
                parent.jclass_list = [];
            }else{
                reader.readAsArrayBuffer(jclasses_file.files[0]);
            }
        },
        start_wasm: function(){
            if( !this.class_name ){
                alert('クラス名を選択してください。');
                return;
            }
            resetInput();
            var inputs = this.input_params.split(',');
            setInput(inputs);

            this.output_return = Module.ccall('callStaticMain', // name of C function 
                                    "number", // return type
                                    ["string", "string"], // argument types
                                    [this.class_name, this.input_arg]); // arguments
            this.output_params = getOutput();
        }
    },
    created: function(){
    },
    mounted: function(){
        proc_load();
    }
};
vue_add_methods(vue_options, methods_utils);
var vue = new Vue( vue_options );

function array_copy(src, src_offset, dest, dest_offset, length){
    for( var i = 0 ; i < length ; i++ )
        dest[dest_offset + i] = src[src_offset + i];
}

function utils_setUint32b(array, offset, value){
	array[offset] = ( value >> 24 ) & 0xff;
	array[offset + 1] = ( value >> 16 ) & 0xff;
	array[offset + 2] = ( value >> 8 ) & 0xff;
	array[offset + 3] = ( value >> 0 ) & 0xff;
}

function utils_setUint16b(array, offset, value){
	array[offset] = ( value >> 8 ) & 0xff;
	array[offset + 1] = ( value >> 0 ) & 0xff;
}

function jar2bin(buffer){
	var jar = new Uint8Array(buffer);
	var unzip = new Zlib.Unzip(jar);
	var fileNames = unzip.getFilenames();
	var encoder = new TextEncoder();

	var classFiles = [];
	var imageSize = 0;
	for (var i = 0 ; i < fileNames.length; ++i) {
		if( !fileNames[i].endsWith('.class') )
			continue;
		var className = fileNames[i].slice(0, -6);
		var classInfo = {
			name: className,
			name_bin : encoder.encode(className),
			binary: unzip.decompress(fileNames[i])
		};
		classFiles.push(classInfo);
		imageSize += 4 + 2 + classInfo.name_bin.length + 4 + classInfo.binary.length;
	}

	var array = new Uint8Array(imageSize);
	var ptr = 0;
	for( var i = 0 ; i < classFiles.length ; i++ ){
		var total = 2 + classFiles[i].name_bin.length + 4 + classFiles[i].binary.length;
		utils_setUint32b( array, ptr, total );
		ptr += 4;
		utils_setUint16b( array, ptr, classFiles[i].name_bin.length );
		ptr += 2;
		array_copy(classFiles[i].name_bin, 0, array, ptr, classFiles[i].name_bin.length);
		ptr += classFiles[i].name_bin.length;
		utils_setUint32b( array, ptr, classFiles[i].binary.length );
		ptr += 4;
		array_copy(classFiles[i].binary, 0, array, ptr, classFiles[i].binary.length);
		ptr += classFiles[i].binary.length;
	}
	
	var ret = {
        array: array,
        names: []
    };
    for( var i = 0 ; i < classFiles.length ; i++ ){
        ret.names.push(classFiles[i].name);
    }

    return ret;
}

function resetInput(){
    if( g_buffer == null ){
        g_buffer = Module._malloc(BUFFER_SIZE);
        var ret = Module.ccall('setInoutBuffer', "number", ["number", "number"], [g_buffer, BUFFER_SIZE]);
        console.log("setInoutBuffer ret=" + ret);        
    }
    Module.HEAPU8[g_buffer] = 0;
}

function setInput(params){
    resetInput();

    var ptr = 1;
    for( var i = 0 ; i < params.length ; i++ ){
        var len = Module.stringToUTF8Array(params[i], Module.HEAPU8, g_buffer + ptr, BUFFER_SIZE - ptr);
        ptr += len;
        Module.HEAPU8[g_buffer + ptr] = 0;
        ptr++;
    }
    Module.HEAPU8[g_buffer] = params.length;
}

function getOutput(){
    var num = Module.HEAPU8[g_buffer];

    var encoder = new TextEncoder();
    var params = [];
    var ptr = 1;
    for( var i = 0 ; i < num ; i++ ){
        var str = Module.UTF8ArrayToString(Module.HEAPU8, g_buffer + ptr, BUFFER_SIZE - ptr);
        params.push(str);
        ptr += encoder.encode(str).length + 1;
    }

    return params;
}
