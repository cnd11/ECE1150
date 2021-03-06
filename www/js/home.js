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
		if(data != "")
		    $("#entry_table tr:last").after(data);
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

function fetch_file() {
    var response = arguments[0];
    var file = arguments[1];

    if( response != "" && response != "FAILURE" )
    {
	$("tr[id*='R" +file+"'] #link a").attr("href","/data"+arguments[1]);
    }
}


function delete_file_res() {
    var success = arguments[0];
    var file = arguments[1];

    if( success != "" && success != "FAILURE" && success != "no" )
    {
	$("tr[id*='R" + file +"']").remove();
    }
    
    if( success == "fd" || success == "no")
	alert(file + " not deleted!");
    else
	alert(file + " deleted!");
}


function search_file_res() {
    var res = (arguments[0]).trim();
    var search = (arguments[1]).trim();

    if( res == "" || res == "FAILURE" || res == "no" || res == "fd")
	alert(search + " not found!" );

    if( res == "ok" ) 
	alert(search + " found!" );    
}

function rename_file_res() {
    var res = arguments[0];
    var requested = arguments[1];
    var replacement = arguments[2];

    $("tr[id*='R" + requested +"']").remove();
    $("#entry_table tr:last").after(replacement);
    alert(arguments[0] );
}
