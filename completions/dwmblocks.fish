# fish completion for dwmblocks
# Install as: ~/.config/fish/completions/dwmblocks.fish
#         or: /usr/share/fish/vendor_completions.d/dwmblocks.fish

function __dwmblocks_names
    dwmblocks --list 2>/dev/null
end

# No positional arguments take files.
complete -c dwmblocks -f

complete -c dwmblocks -l all      -d 'Update all blocks'
complete -c dwmblocks -l restart  -d 'Restart the daemon'
complete -c dwmblocks -l list     -d 'List configured block names'
complete -c dwmblocks -l update   -d 'Update a block by name' -x -a '(__dwmblocks_names)'
complete -c dwmblocks -l version  -d 'Print version'
complete -c dwmblocks -l help     -d 'Print help'
