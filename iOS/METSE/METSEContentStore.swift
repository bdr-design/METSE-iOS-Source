import Foundation

struct METSEBootstrap: Decodable {
    struct EngineTuning: Decodable {
        let targetFPS: Int
        let fixedStepHz: Int
        let maxCatchUpSteps: Int
        let maxCombatants: Int
    }

    let contentSchema: Int
    let contentVersion: String
    let sequence: Int
    let channel: String
    let updateManifestURL: String
    let trustedUpdateHosts: [String]
    let engineTuning: EngineTuning
}

enum METSEContentStore {
    static func bootstrap() throws -> METSEBootstrap {
        guard let url = Bundle.main.url(forResource: "bootstrap", withExtension: "json") else {
            throw NSError(domain: "METSE.Content", code: 1, userInfo: [NSLocalizedDescriptionKey: "bootstrap.json مفقود"])
        }
        return try JSONDecoder().decode(METSEBootstrap.self, from: Data(contentsOf: url))
    }
}
