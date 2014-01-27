$( function() {
    $(document).ajaxStart( function() {
        $('.printer_function').prop('disabled',true);
    })
    $(document).ajaxStop( function() {
        $('.printer_function').prop('disabled',false);
    })
})