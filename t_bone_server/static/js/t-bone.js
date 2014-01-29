$(function () {
    $(document).ajaxStart(function () {
        $('.printer_function').prop('disabled', true);
    })
    $(document).ajaxStop(function () {
        $('.printer_function').prop('disabled', false);
    })
})

var last_status;
function update_status() {
    $.getJSON("/status", function (status_data) {
        last_status = status_data;
        $("body").trigger({
            type: "status_update",
            status_data: status_data
        });
    })
}

$().ready(function () {
    setInterval("update_status()", 5000);
})
