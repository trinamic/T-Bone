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
            $("#extruder-temperature-text").text(eventData.status_data.extruder_temperature);
            $("#extruder-temperature-page-text").text(eventData.status_data.extruder_temperature);
            $("#extruder-set-temperature-text").text(eventData.status_data.extruder_set_temperature);
            if (typeof eventData.status_data.bed_temperature != "undefined") {
                $("#bed-temperature-text").text(eventData.status_data.bed_temperature);
                $("#bed-temperature-page-text").text(eventData.status_data.bed_temperature);
                $("#bed-set-temperature-text").text(eventData.status_data.bed_set_temperature);
            }
            var axis_name;
            for (axis_name in eventData.status_data.axis_status) {
                var axis_status = eventData.status_data.axis_status[axis_name];
                $("." + axis_name + "_pos").text(axis_status['position'].toFixed(2));
                $("." + axis_name + "_encoder_pos").text(axis_status['encoder_pos'].toFixed(2));
                if (axis_status['left_endstop']) {
                    $("." + axis_name + "_left_endstop").show();
                } else {
                    $("." + axis_name + "_left_endstop").hide();
                }
                if (axis_status['right_endstop']) {
                    $("." + axis_name + "_right_endstop").show();
                } else {
                    $("." + axis_name + "_right_endstop").hide();
                }
            }
            if (eventData.status_data.printing) {
                $("#printing-progress").show();
                $("#printing-progress-bar").width(eventData.status_data.lines_printed_percent + "%");
                $("#queue-status-progress").show();
                $("#queue-status-progress-bar").width(eventData.status_data.queue_percentage + "%");
            } else {
                $("#printing-progress").hide();
                $("#queue-status-progress").hide();
            }
        }
    )
});