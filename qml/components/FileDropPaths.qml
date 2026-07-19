pragma Singleton
import QtQuick

QtObject {
    function localPath(url) {
        if (url === undefined || url === null)
            return ""
        if (typeof url !== "string" && typeof url.toLocalFile === "function") {
            const local = url.toLocalFile()
            if (local.length > 0)
                return local
        }
        let s = typeof url === "string" ? url : (url.toString ? url.toString() : "")
        if (s.startsWith("file://")) {
            const resolved = Qt.resolvedUrl(s)
            if (resolved && typeof resolved.toLocalFile === "function") {
                const local = resolved.toLocalFile()
                if (local.length > 0)
                    return local
            }
            if (s.startsWith("file:///"))
                s = s.slice(8)
            else if (s.startsWith("file://"))
                s = s.slice(7)
            try {
                s = decodeURIComponent(s)
            } catch (e) {
            }
            if (s.length > 2 && s.charAt(0) === "/" && s.charAt(2) === ":")
                s = s.slice(1)
        }
        return s
    }

    function pathsFromDrop(drop) {
        const paths = []
        if (!drop)
            return paths

        if (drop.hasUrls && drop.urls) {
            for (let i = 0; i < drop.urls.length; ++i) {
                const local = localPath(drop.urls[i])
                if (local.length > 0)
                    paths.push(local)
            }
        }

        if (paths.length === 0 && drop.hasText && drop.text) {
            const lines = drop.text.split(/\r?\n/)
            for (let j = 0; j < lines.length; ++j) {
                const local = localPath(lines[j].trim())
                if (local.length > 0)
                    paths.push(local)
            }
        }
        return paths
    }

    function acceptDrop(drop, onPaths) {
        if (!drop)
            return
        drop.accept(Qt.CopyAction)
        const paths = pathsFromDrop(drop)
        if (paths.length > 0 && onPaths)
            onPaths(paths)
    }
}
