var blocked = false

function blocking_ajax(url) {
    if (!blocked) {
        $('.printer_actions').disable();
        $.ajax({
            url: url,
            context: document.body,
            error: function () {
                blocked = false;
            }
        }).done(function () {
                $('.printer_actions').enable();
                blocked = false;
            });
    }

}