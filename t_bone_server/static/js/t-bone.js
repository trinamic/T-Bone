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
            $("#queue-status-progress-bar").width(eventData.status_data.queue_percentage + "%");
            $("#printing_status_text").text(eventData.status_data.print_status);
            var printing_progress_bar = $("#printing_progress_bar")
            if (eventData.status_data.printing) {
                printing_progress_bar.show();
                printing_progress_bar.width(eventData.status_data.lines_printed_percent + "%")
            } else {
                printing_progress_bar.hide();
            }
        }
    )
})