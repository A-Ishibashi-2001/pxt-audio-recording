input.onButtonPressed(Button.A, function () {
    record.setSampleRate(11000, record.AudioSampleRateScope.Recording)
    record.startRecording(record.BlockingState.Nonblocking)
})
input.onButtonPressed(Button.B, function () {
    record.setSampleRate(11000, record.AudioSampleRateScope.Playback)
    record.playAudio(record.BlockingState.Nonblocking)
})
input.onLogoEvent(TouchButtonEvent.Pressed, function () {
    record.sendToSerial(record.BlockingState.Nonblocking)
})
//serial.setTxBufferSize(254)
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
