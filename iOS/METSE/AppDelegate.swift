import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil) -> Bool {
        METSECrashReporter.install()
        METSEDiagnosticRecorder.shared.start()
        let window = UIWindow(frame: UIScreen.main.bounds)
        let navigation = UINavigationController(rootViewController: GatewayViewController())
        navigation.navigationBar.prefersLargeTitles = false
        window.rootViewController = navigation
        window.makeKeyAndVisible()
        self.window = window
        return true
    }
}
