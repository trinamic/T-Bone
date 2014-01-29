$(function () {
    $(document).ajaxStart(function () {
        $('.printer_function').prop('disabled', true);
    })
    $(document).ajaxStop(function () {
        $('.printer_function').prop('disabled', false);
    })
})

var last_status;

$().ready(function () {
    $("body").timer(function () {
            $.getJSON("/status", function (status_data) {
                last_status = status_data
                $("body").trigger({
                    type: "status_update",
                    status_data: status_data
                });
            })
        },
        delay = 1000,
        repeat = true
    )
})
