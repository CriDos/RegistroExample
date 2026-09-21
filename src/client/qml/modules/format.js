function formatDate(isoStr) {
    if (!isoStr || isoStr.length === 0)
        return "—";
    var d = new Date(isoStr);
    if (isNaN(d.getTime()))
        return isoStr;
    function pad(n) {
        return n < 10 ? "0" + n : "" + n;
    }
    return pad(d.getDate()) + "." + pad(d.getMonth() + 1) + "." + d.getFullYear() + " " + pad(d.getHours()) + ":" + pad(d.getMinutes());
}

function phoneDigits(value) {
    return (value || "").replace(/\D/g, "");
}

function normalizePhone(raw) {
    if (!raw)
        return "";
    var digits = phoneDigits(raw);
    if (digits.length === 10)
        digits = "7" + digits;
    if (digits.length === 11 && (digits.charAt(0) === "7" || digits.charAt(0) === "8"))
        digits = "7" + digits.substring(1);
    if (digits.length === 11 && digits.charAt(0) === "7")
        return "+7 (" + digits.substring(1, 4) + ") " + digits.substring(4, 7) + "-" + digits.substring(7, 9) + "-" + digits.substring(9, 11);
    return (raw || "").trim();
}
