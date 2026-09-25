import Foundation
import Vision
import AppKit
let cases = [
 ("wifi", "WIFI:T:WPA;S:XiaoLiuRen-TEST;P:0123ABCD;;"),
 ("page", "http://192.168.4.1"),
 ("dpp", "DPP:C:81/6;M:001122334455;I:XiaoLiuRen;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgAC7Wq8u7goDGmsCGzpdd-zNkBZNdPlLRwsToCQsiFCqXs;;")
]
for (name, expected) in cases {
 let request = VNDetectBarcodesRequest()
 request.symbologies = [.qr]
 try VNImageRequestHandler(url: URL(fileURLWithPath: "/private/tmp/xlr-\(name)-qr.bmp")).perform([request])
 guard request.results?.first?.payloadStringValue == expected else { fatalError("QR decode failed: \(name)") }
 print("\(name) QR decoded correctly")
}
