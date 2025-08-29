input.onButtonPressed(Button.A, function () {
    record.setSampleRate(11000, record.AudioSampleRateScope.Recording)
    record.startRecording(record.BlockingState.Blocking)
})
input.onButtonPressed(Button.B, function () {
    record.setSampleRate(11000, record.AudioSampleRateScope.Playback)
    record.playAudio(record.BlockingState.Blocking)
})
input.onLogoEvent(TouchButtonEvent.Pressed, function () {
    record.sendToSerial()
})
serial.setTxBufferSize(254)
basic.forever(function () {
    if (record.sendingToSerial()) {
        basic.showIcon(IconNames.Diamond)
    } else if (record.audioStatus(record.AudioStatus.Playing)) {
        basic.showLeds(`
            . # . . .
            . # # . .
            . # # # .
            . # # . .
            . # . . .
            `)
    } else if (record.audioStatus(record.AudioStatus.Recording)) {
        basic.showIcon(IconNames.SmallSquare)
    } else {
        basic.clearScreen()
    }
})
