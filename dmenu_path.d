import std;

void main() {
    std.process.environment
    .get("PATH")
    .split(':')
    .filter!(std.file.exists)
    .map!(p => p
            .dirEntries(SpanMode.shallow)
            .filter!(ent => ent.isFile)
            .filter!(ent => ent.attributes & std.conv.octal!"100")
            .map!(ent => std.path.baseName(ent.name))
            )
    .filter!(a => !a.empty)
    .joiner()
    .array
    .sort
    .uniq
    .each!writeln;
}
