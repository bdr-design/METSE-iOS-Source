import Foundation

/// Persisted player preferences. UserDefaults is intentionally used instead of a custom
/// file format: this is a handful of scalar/bool preferences, not gameplay state, and it
/// must stay strictly UI/persistence-only - never reach into the simulation core or its
/// single state owner (see guardrails_012s.py).
enum METSESettings {
    private enum Key: String {
        case lookSensitivity = "metse.settings.lookSensitivity"
        case adsSensitivity = "metse.settings.adsSensitivity"
        case gyroscopeEnabled = "metse.settings.gyroscopeEnabled"
        case gyroscopeSensitivity = "metse.settings.gyroscopeSensitivity"
        case leftHandedLayout = "metse.settings.leftHandedLayout"
        case hudOpacity = "metse.settings.hudOpacity"
    }

    // Defaults chosen to match the existing hardcoded constants in GameViewController
    // (0.0040 yaw / 0.00335 pitch) so shipping this does not silently change feel for
    // anyone who never opens Settings.
    static let lookSensitivityRange: ClosedRange<Double> = 0.4...2.0
    static let adsSensitivityRange: ClosedRange<Double> = 0.4...2.0
    static let gyroscopeSensitivityRange: ClosedRange<Double> = 0.2...2.0

    static var lookSensitivity: Double {
        get { value(.lookSensitivity, default: 1.0, in: lookSensitivityRange) }
        set { UserDefaults.standard.set(newValue.clamped(to: lookSensitivityRange), forKey: Key.lookSensitivity.rawValue) }
    }

    static var adsSensitivity: Double {
        get { value(.adsSensitivity, default: 1.0, in: adsSensitivityRange) }
        set { UserDefaults.standard.set(newValue.clamped(to: adsSensitivityRange), forKey: Key.adsSensitivity.rawValue) }
    }

    static var gyroscopeEnabled: Bool {
        get { UserDefaults.standard.object(forKey: Key.gyroscopeEnabled.rawValue) as? Bool ?? false }
        set { UserDefaults.standard.set(newValue, forKey: Key.gyroscopeEnabled.rawValue) }
    }

    static var gyroscopeSensitivity: Double {
        get { value(.gyroscopeSensitivity, default: 1.0, in: gyroscopeSensitivityRange) }
        set { UserDefaults.standard.set(newValue.clamped(to: gyroscopeSensitivityRange), forKey: Key.gyroscopeSensitivity.rawValue) }
    }

    static var leftHandedLayout: Bool {
        get { UserDefaults.standard.object(forKey: Key.leftHandedLayout.rawValue) as? Bool ?? false }
        set { UserDefaults.standard.set(newValue, forKey: Key.leftHandedLayout.rawValue) }
    }

    static var hudOpacity: Double {
        get { value(.hudOpacity, default: 1.0, in: 0.4...1.0) }
        set { UserDefaults.standard.set(newValue.clamped(to: 0.4...1.0), forKey: Key.hudOpacity.rawValue) }
    }

    static func resetToDefaults() {
        for key in [Key.lookSensitivity, .adsSensitivity, .gyroscopeEnabled, .gyroscopeSensitivity, .leftHandedLayout, .hudOpacity] {
            UserDefaults.standard.removeObject(forKey: key.rawValue)
        }
    }

    private static func value(_ key: Key, default defaultValue: Double, in range: ClosedRange<Double>) -> Double {
        guard let stored = UserDefaults.standard.object(forKey: key.rawValue) as? Double else { return defaultValue }
        return stored.clamped(to: range)
    }
}

private extension Double {
    func clamped(to range: ClosedRange<Double>) -> Double { Swift.min(Swift.max(self, range.lowerBound), range.upperBound) }
}
