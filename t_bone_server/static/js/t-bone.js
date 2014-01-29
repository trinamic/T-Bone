var background_update = false;

$(function () {
    $(document).ajaxStart(function () {
        if (!background_update) {
            $('.printer_function').prop('disabled', true);
        }
    })
    $(document).ajaxStop(function () {
        $('.printer_function').prop('disabled', false);
    })
})

var last_status;
function update_status() {
    background_update = true;
    $.getJSON("/status", function (status_data) {
        background_update = false;
        last_status = status_data;
        $("body").trigger({
            type: "status_update",
            status_data: status_data
        });
    })
}

$().ready(function () {
    setInterval("update_status()", 1000);
})

//update the status bar
$().ready(function () {
    $("body").bind(
        "status_update",
        function (eventData) {
            $("#printing-status-text").text(eventData.status_data.print_status);
            if (eventData.status_data.printing) {
                $("#printing-progress").show();
                $("#printing-progress-bar").width(eventData.status_data.lines_printed_percent + "%");
                $("queue-status-progress").show();
                $("queue-status-progress-bar").width(eventData.status_data.queue_percentage + "%");
            } else {
                $("#printing-progress").hide();
                $("queue-status-progress").hide();
            }
        }
    )
})