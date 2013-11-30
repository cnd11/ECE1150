$(document).ready(function() {
    setTimeout(function(){
	$("#welcome").remove();
    }, 5000);

    $('#submit_upload').click(function() {
	var form_data = new FormData($('form')[0]);
	
	$.ajax({
	    type: 'POST',
	    url: '/cgi-bin/upload.pl',
	    data: form_data,
	    success: function(data) {
		alert("Upload!");
		$("#uploads").append(data);
	    },
	    error: function(data) {
		alert("Failure! ");
		console.log("my obj %o", data);
	    },
	    cache: false,
	    contentType: false,
	    processData: false,
	});
    });
});

function disappear() {
    var para = arguments[0];
    
    $("#disp").append(para);
    setTimeout(function(){
	$("#goAway").remove();
    }, 5000);
} 
