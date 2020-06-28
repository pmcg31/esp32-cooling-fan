class DashMeter {
    constructor(canvas, maxValue) {
        this._canvas = canvas;
        this._maxValue = maxValue;
        this._value = 0;
        this._previousValue = 0;
        this._label = "";
        this._background = "#ffffff";
        this._foreground = "#000000";
        this._unfilledColor = "#cccccc";
        this._smallTextColor = "#cccccc";
        this._arcStart = Math.PI / 4 + Math.PI / 2;
        this._arcLen = Math.PI * 2 * 0.75;
        this._lineWidth = this._canvas.width / 27;
        this._largeFontSize = Math.round(this._canvas.width * 0.15);
        this._fontFamily = "Arial";
        this._fontWeight = "normal";
        this.updateFonts();
        this._radius = (this._canvas.width * 0.75) / 2;
        let sagitta = this._radius * (1 - Math.cos(Math.PI / 4));
        let arcHeight = this._radius * 2 - sagitta;
        this._center_x = this._canvas.width / 2;
        this._center_y = this._canvas.height / 2;
        this._arc_y = this._center_y + sagitta / 2;
    }

    get value() {
        return this._value;
    }

    set value(newValue) {
        this._previousValue = this._value;
        this._value = newValue;
    }

    get label() {
        return this._label;
    }

    set label(newLabel) {
        this._label = newLabel;
    }

    get background() {
        return this._background;
    }

    set background(color) {
        this._background = color;
    }

    get foreground() {
        return this._foreground;
    }

    set foreground(color) {
        this._foreground = color;
    }

    get unfilledColor() {
        return this._unfilledColor;
    }

    set unfilledColor(color) {
        this._unfilledColor = color;
    }

    get smallTextColor() {
        return this._smallTextColor;
    }

    set smallTextColor(color) {
        this._smallTextColor = color;
    }

    get fontFamily() {
        return this._fontFamily;
    }

    set fontFamily(fontFamily) {
        this._fontFamily = fontFamily;
        this.updateFonts();
    }

    get fontWeight() {
        return this._fontWeight;
    }

    set fontWeight(fontWeight) {
        this._fontWeight = fontWeight;
        this.updateFonts();
    }

    draw() {
        // Update anything that depends on the canvas size
        this.updateSizes();

        // Determine start and end ratio for filled-in part of arc
        let startRatio = this._previousValue / this._maxValue;
        let endRatio = this._value / this._maxValue;

        // Grab a 2D drawing context
        let context = this._canvas.getContext("2d");

        // Clear canvas
        context.fillStyle = this._background;
        context.fillRect(0, 0, this._canvas.width, this._canvas.height);

        // Draw text
        this.drawText(context);

        // Draw initial arc frame
        this.drawArc(context, startRatio);

        // Animate arc (60fps, 2/3 sec)
        var ratio = startRatio;
        var timerId = setInterval(frame, (1.0 / 60.0) * 1000.0);
        let step = (endRatio - startRatio) / (60 * 0.66);
        let meter = this;
        function frame() {
            if (step > 0) {
                if (ratio < endRatio) {
                    meter.drawArc(context, ratio);
                    ratio += step;
                } else {
                    clearInterval(timerId);
                    meter.drawArc(context, endRatio);
                }
            } else {
                if (ratio > endRatio) {
                    meter.drawArc(context, ratio);
                    ratio += step;
                } else {
                    clearInterval(timerId);
                    meter.drawArc(context, endRatio);
                }
            }
        }
    }

    drawArc(context, ratio) {
        // Set up brush
        context.lineWidth = this._lineWidth;
        context.lineCap = "round";

        // Draw "empty" meter
        context.strokeStyle = this._unfilledColor;
        context.beginPath();
        context.arc(
            this._center_x,
            this._arc_y,
            this._radius,
            this._arcStart,
            this._arcStart + this._arcLen,
            false
        );
        context.stroke();

        // Fill in empty meter according to ratio
        context.strokeStyle = this._foreground;
        var pctArcLen = this._arcLen * ratio;
        context.beginPath();
        context.arc(
            this._center_x,
            this._arc_y,
            this._radius,
            this._arcStart,
            this._arcStart + pctArcLen,
            false
        );
        context.stroke();
    }

    drawText(context) {
        // Set up for large font
        context.font = this._largeFont;

        // Measure the text for the value
        var metrics = context.measureText("0123456789");
        let fontHeight =
            metrics.actualBoundingBoxAscent + metrics.actualBoundingBoxDescent;
        let baselineOffset = metrics.actualBoundingBoxAscent;
        metrics = context.measureText(this._value);
        let valueTextWidth = metrics.width;

        // Set up for small font
        context.font = this._smallFont;

        // Measure the text for the label
        metrics = context.measureText(this._label);
        let labelTextWidth = metrics.width;

        // Determine space between value and label, measure
        // the whole mess and figure out where to draw
        // so it comes out centered
        let x = this._center_x;
        let y = this._arc_y - (this._arc_y - this._center_y) / 2;
        let spacer = fontHeight / 10;
        let textWidth = valueTextWidth + spacer + labelTextWidth;
        let textX = x - textWidth / 2;
        let textY = y - fontHeight / 2 + baselineOffset;

        // Draw value (larger font, brighter color)
        context.font = this._largeFont;
        context.fillStyle = this._foreground;
        context.fillText(this._value, textX, textY);

        // Draw the label (smaller font, subtle color)
        context.font = this._smallFont;
        context.fillStyle = this._smallTextColor;
        context.fillText(this._label, textX + valueTextWidth + spacer, textY);
    }

    updateSizes() {
        this._lineWidth = this._canvas.width / 27;
        this._largeFontSize = Math.round(this._canvas.width * 0.15);
        this._radius = (this._canvas.width * 0.75) / 2;
        let sagitta = this._radius * (1 - Math.cos(Math.PI / 4));
        let arcHeight = this._radius * 2 - sagitta;
        this._center_x = this._canvas.width / 2;
        this._center_y = this._canvas.height / 2;
        this._arc_y = this._center_y + sagitta / 2;
    }

    updateFonts() {
        this._largeFont = this._fontWeight + " " + this._largeFontSize + "px " + this._fontFamily;
        this._smallFont = this._fontWeight + " " + (this._largeFontSize / 2) + "px " + this._fontFamily;
    }
}
