import AppKit
let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 760, pixelsHigh: 460,
                             bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
                             isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
NSColor(calibratedRed: 16/255, green: 20/255, blue: 24/255, alpha: 1).setFill()
NSBezierPath(rect: NSRect(x: 0, y: 0, width: 760, height: 460)).fill()
func text(_ string: String, _ x: CGFloat, _ y: CGFloat, _ size: CGFloat, _ color: NSColor) {
    (string as NSString).draw(at: NSPoint(x: x, y: y), withAttributes: [
        .font: NSFont.systemFont(ofSize: size, weight: size > 30 ? .bold : .regular), .foregroundColor: color])
}
let white = NSColor(calibratedWhite: 0.93, alpha: 1)
let muted = NSColor(calibratedRed: 0.51, green: 0.58, blue: 0.62, alpha: 1)
let mint = NSColor(calibratedRed: 0.51, green: 0.94, blue: 0.77, alpha: 1)
text("aura", 38, 396, 36, white)
text("SPECTRAL HARMONY", 138, 409, 12, mint)
text("Drag each plug-in to its matching folder.", 38, 365, 15, muted)
text("→", 355, 269, 32, mint)
text("→", 355, 129, 32, mint)
text("Logic Pro: AU   ·   FL Studio: VST3 or AU   ·   Universal 2", 38, 35, 12, muted)
NSGraphicsContext.restoreGraphicsState()
try bitmap.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))
